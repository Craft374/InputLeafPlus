/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "TabBridge.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>

namespace {

// the tablet's own value of "포인터 정밀도 향상" while a session has it switched off; removed once
// the tablet has put it back
const char* const kRestoreKey = "tabRestorePointerPrecision";

QString savedPrecision()
{
    const QString value = QSettings().value(kRestoreKey).toString();
    return QRegularExpression(QStringLiteral("^(-?\\d+|null)$")).match(value).hasMatch() ? value : QString();
}

QString findAdb()
{
    // a Finder-launched app has no Homebrew PATH. Prefer one adb consistently: two different adb
    // versions keep killing each other's server.
    const QStringList candidates = {QStringLiteral("/opt/homebrew/bin/adb"), QStringLiteral("/usr/local/bin/adb"),
                                    QDir::homePath() + QStringLiteral("/Library/Android/sdk/platform-tools/adb")};
    for (const QString& path : candidates) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return QStandardPaths::findExecutable(QStringLiteral("adb"));
}

} // namespace

TabBridge::TabBridge(QObject* parent) :
    QObject(parent),
    m_socket(new QTcpSocket(this)),
    m_retry(new QTimer(this)),
    m_motion(new QTimer(this))
{
    m_retry->setSingleShot(true);
    // the server sends motion hundreds of times a second; 125 reports a second keep wireless bursts small
    m_motion->setSingleShot(true);
    m_motion->setInterval(8);
    connect(m_motion, &QTimer::timeout, this, [this]() {
        if (!m_session)
            return;
        const QStringList lines = m_session->takeHidLines();
        if (!lines.isEmpty()) {
            writeHid(lines);
            m_motion->start();
        }
    });
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        // one Mac point = one tablet dp at 100%; the server measures the screen in Mac points
        const double pixelsPerPoint = (m_density > 0 ? m_density / 160.0 : 2.0) * m_speedPercent / 100.0;
        m_session = std::make_unique<tab::Session>(screenName(), qRound(m_metrics.width / pixelsPerPoint),
                                                   qRound(m_metrics.height / pixelsPerPoint),
                                                   pixelsPerPoint / m_metrics.gain);
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &TabBridge::onServerData);
    connect(m_socket, &QTcpSocket::disconnected, this, &TabBridge::onServerGone);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this]() {
        if (m_socket->state() == QAbstractSocket::UnconnectedState)
            onServerGone();   // refused while the server is (re)starting: no disconnected() then
    });
}

TabBridge::~TabBridge()
{
    stop();
    if (m_closing)
        m_closing->waitForFinished(3000);   // on quit, give the tablet time to restore its setting
}

QString TabBridge::screenName()
{
    return QStringLiteral("GalaxyTab");
}

void TabBridge::start(const QString& tabAddress, const QString& serverHost, quint16 serverPort, int speedPercent)
{
    stop();
    m_address = tabAddress.trimmed();
    m_serverHost = serverHost;
    m_serverPort = serverPort;
    m_speedPercent = qBound(10, speedPercent, 1000);
    m_adb = findAdb();
    if (m_adb.isEmpty()) {
        report(tr("adb를 찾지 못했습니다. 터미널에서 brew install android-platform-tools 로 설치해 주세요."), false);
        return;
    }
    m_lastStatus.clear();
    m_running = true;
    if (m_closing) {
        // let the previous session put the tablet's setting back before this one switches it off
        connect(m_closing, &QProcess::finished, this, [this]() {
            if (m_running && !m_hid)
                connectAdb();
        });
    } else {
        connectAdb();
    }
}

void TabBridge::stop()
{
    m_running = false;
    m_retry->stop();
    if (m_session) {
        m_session->releaseAll();
        writeHid(m_session->takeHidLines());
        m_session.reset();   // before abort(): onServerGone() must not report a deliberate stop
    }
    m_socket->abort();
    closeHid();
}

void TabBridge::closeHid()
{
    if (!m_hid)
        return;
    // EOF ends `hid` on the tablet, which removes the virtual devices (and anything held), then the
    // same shell puts the pointer setting back. Killing it would skip that.
    QProcess* process = m_hid;
    m_hid = nullptr;
    m_hidReady = false;
    process->closeWriteChannel();
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    m_closing = process;
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    QTimer::singleShot(5000, process, [process]() { process->kill(); });
}

QProcess* TabBridge::adb(const QStringList& args)
{
    auto* process = new QProcess(this);
    process->setProgram(m_adb);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);
    return process;
}

void TabBridge::connectAdb()
{
    if (!m_running)
        return;
    if (m_address.isEmpty()) {
        report(tr("설정에 갤럭시 탭 주소(IP:포트)를 입력해 주세요."), false);
        return;
    }
    QProcess* process = adb({QStringLiteral("connect"), m_address});
    connect(process, &QProcess::finished, this, [this, process]() {
        const QString output = QString::fromUtf8(process->readAll()).trimmed();
        process->deleteLater();
        if (!m_running)
            return;
        if (output.contains(QLatin1String("connected to"))) {   // also "already connected to"
            startHid();
            return;
        }
        report(tr("갤럭시 탭(%1)에 연결하지 못했습니다: %2\n탭의 무선 디버깅이 켜져 있는지, 설정의 주소가 "
                  "무선 디버깅 화면의 'IP 주소 및 포트'와 같은지 확인해 주세요. 10초 뒤 다시 시도합니다.")
                   .arg(m_address, output.section(QLatin1Char('\n'), -1)), false);
        retry(&TabBridge::connectAdb, 10000);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        report(tr("adb를 실행하지 못했습니다: %1").arg(m_adb), false);
        retry(&TabBridge::connectAdb, 30000);
    });
    process->start();
}

void TabBridge::startHid()
{
    if (!m_running || m_hid)
        return;
    // Switch acceleration off, run `hid` until our stdin closes, then put the setting back in the
    // same shell so it is restored in order. A "0" read at the start while a value is still saved
    // means an earlier session never got to restore (tablet dropped off wifi), so restore that one.
    // The setup commands read /dev/null so they can't swallow the JSON lines meant for `hid`.
    QString script = QStringLiteral("p=$(settings get system enhance_pointer_precision </dev/null); echo ILP_PREC=$p; r=$p; ");
    const QString saved = savedPrecision();
    if (!saved.isEmpty())
        script += QStringLiteral("[ \"$p\" = 0 ] && r=%1; ").arg(saved);
    script += QStringLiteral(
        "settings put system enhance_pointer_precision 0 </dev/null; echo ILP_READY; hid -; "
        "if [ \"$r\" = null ]; then settings delete system enhance_pointer_precision; "
        "else settings put system enhance_pointer_precision \"$r\"; fi");
    QProcess* process = adb({QStringLiteral("-s"), m_address, QStringLiteral("shell"), script});
    m_hid = process;
    m_hidReady = false;
    m_hidOutput.clear();
    connect(process, &QProcess::readyReadStandardOutput, this, &TabBridge::onHidOutput);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        // the restore is the shell's last command: a clean 0 means the tablet has its own value back
        if (status == QProcess::NormalExit && exitCode == 0)
            QSettings().remove(kRestoreKey);
        if (process == m_hid)
            onHidGone();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && process == m_hid)
            onHidGone();
    });
    process->start();
}

void TabBridge::onHidOutput()
{
    if (!m_hid)
        return;
    m_hidOutput.append(m_hid->readAllStandardOutput());
    int newline;
    while ((newline = m_hidOutput.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_hidOutput.left(newline)).trimmed();
        m_hidOutput.remove(0, newline + 1);
        if (line.startsWith(QLatin1String("ILP_PREC="))) {
            // kept in case this session can't restore it; "0" is either the user's own choice
            // (nothing to keep) or an unrestored earlier session (keep what that one saved)
            const QString value = line.mid(9);
            if (value != QLatin1String("0") && QRegularExpression(QStringLiteral("^(-?\\d+|null)$")).match(value).hasMatch())
                QSettings().setValue(kRestoreKey, value);
        } else if (line == QLatin1String("ILP_READY") && !m_hidReady) {
            m_hidReady = true;
            writeHid(tab::registerLines());
            QTimer::singleShot(1000, this, [this]() { queryMetrics(1); });
        }
    }
    if (m_hidOutput.size() > 4096)
        m_hidOutput.clear();   // `hid` chatter without newlines; nothing we need
}

void TabBridge::queryMetrics(int attempt)
{
    if (!m_running || !m_hid)
        return;
    // shortcut: size and gain are read once per connection; rotating the tablet needs a reconnect
    // (toggle the setting or press Reload).
    QProcess* process = adb({QStringLiteral("-s"), m_address, QStringLiteral("shell"),
                             QStringLiteral("dumpsys input; echo; wm density")});
    connect(process, &QProcess::finished, this, [this, process, attempt]() {
        const QString output = QString::fromUtf8(process->readAll());
        process->deleteLater();
        if (!m_running || !m_hid)
            return;
        tab::Metrics metrics;
        if (!tab::parseMetrics(output, tab::mouseDeviceName(), &metrics)) {
            if (attempt < 5) {
                QTimer::singleShot(1000, this, [this, attempt]() { queryMetrics(attempt + 1); });
                return;
            }
            report(tr("갤럭시 탭에서 가상 마우스를 확인하지 못했습니다. 다시 연결합니다."), false);
            closeHid();
            retry(&TabBridge::connectAdb, 10000);
            return;
        }
        if (!metrics.linear) {
            report(tr("갤럭시 탭의 마우스 가속이 꺼지지 않았습니다. 화면을 넘나들 때 커서 위치가 조금 어긋날 수 "
                      "있습니다."), false);
        }
        m_metrics = metrics;
        m_density = tab::parseDensity(output);
        connectServer();
    });
    process->start();
}

void TabBridge::connectServer()
{
    if (!m_running || !m_hid || m_socket->state() != QAbstractSocket::UnconnectedState)
        return;
    m_socket->connectToHost(m_serverHost, m_serverPort);
}

void TabBridge::onServerData()
{
    if (!m_session)
        return;
    const bool wasAccepted = m_session->accepted();
    const QByteArray reply = m_session->feed(m_socket->readAll());
    if (!reply.isEmpty())
        m_socket->write(reply);
    // motion right away after a pause, then once per tick; clicks and keys always go now
    // (a click sends the motion before it first)
    const bool tick = !m_motion->isActive();
    writeHid(m_session->takeHidLines(tick));
    if (tick)
        m_motion->start();
    if (m_session->failed()) {
        m_socket->abort();   // onServerGone() reports the error and schedules the retry
        return;
    }
    if (m_session->accepted() && !wasAccepted)
        report(tr("갤럭시 탭이 연결됐습니다 (%1×%2).").arg(m_metrics.width).arg(m_metrics.height), true);
}

void TabBridge::onServerGone()
{
    int delay = 2000;
    if (m_session) {
        m_session->releaseAll();
        writeHid(m_session->takeHidLines());
        if (m_session->failed()) {
            report(m_session->error(), false);
            delay = 10000;
        } else if (!m_session->accepted()) {
            // the server drops an unknown screen name without saying so
            report(tr("서버가 갤럭시 탭 화면(%1)을 받아주지 않았습니다. 화면 배치 설정에 %1 화면이 있는지 확인해 "
                      "주세요.").arg(screenName()), false);
            delay = 10000;
        } else if (m_running) {
            report(tr("갤럭시 탭과 서버의 연결이 끊겼습니다. 다시 연결합니다."), false);
        }
        m_session.reset();
    }
    if (!m_running || !m_hid || m_retry->isActive())
        return;
    retry(&TabBridge::connectServer, delay);
}

void TabBridge::onHidGone()
{
    QProcess* process = m_hid;
    m_hid = nullptr;
    m_hidReady = false;
    if (process)
        process->deleteLater();
    m_session.reset();   // its devices are gone, nothing to release
    m_socket->abort();   // a screen without its devices would trap the cursor on the tablet
    if (!m_running)
        return;
    report(tr("갤럭시 탭과의 adb 연결이 끊겼습니다. 다시 연결합니다."), false);
    retry(&TabBridge::connectAdb, 5000);
}

void TabBridge::writeHid(const QStringList& lines)
{
    if (!m_hid || !m_hidReady || lines.isEmpty())
        return;
    m_hid->write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
}

void TabBridge::retry(void (TabBridge::*step)(), int ms)
{
    if (!m_running)
        return;
    m_retry->disconnect();
    connect(m_retry, &QTimer::timeout, this, step);
    m_retry->start(ms);
}

void TabBridge::report(const QString& message, bool ok)
{
    if (message == m_lastStatus)
        return;
    m_lastStatus = message;
    emit status(message, ok);
}
