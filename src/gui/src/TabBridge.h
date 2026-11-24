/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "TabBridgeCore.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QProcess;
class QTcpSocket;
class QTimer;

// Puts an Android tablet (Galaxy Tab in DeX) on the local InputLeafPlus server as one more screen.
// Over wireless adb it runs the tablet's stock `hid` tool, which creates a real virtual mouse and
// keyboard, then joins the server as the client screenName() and forwards what the server sends.
// Nothing is installed on the tablet. While connected the tablet's mouse acceleration
// ("포인터 정밀도 향상") is off so the cursor follows the server exactly; the tablet restores it
// itself when the session ends.
class TabBridge : public QObject {
    Q_OBJECT
public:
    explicit TabBridge(QObject* parent = nullptr);
    ~TabBridge() override;

    static QString screenName();

    // tabAddress: "ip:port" from the tablet's wireless debugging screen (already paired).
    // speedPercent: 100 = one Mac point moves one tablet dp.
    void start(const QString& tabAddress, const QString& serverHost, quint16 serverPort, int speedPercent);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void status(const QString& message, bool ok);   // Korean, only when it changes

private:
    void connectAdb();
    void startHid();
    void queryMetrics(int attempt);
    void connectServer();
    void onHidOutput();
    void onHidGone();
    void onServerData();
    void onServerGone();
    void writeHid(const QStringList& lines);
    void closeHid();
    void retry(void (TabBridge::*step)(), int ms);
    void report(const QString& message, bool ok);
    QProcess* adb(const QStringList& args);

    bool m_running = false;
    QString m_adb;
    QString m_address;
    QString m_serverHost;
    quint16 m_serverPort = 0;
    int m_speedPercent = 100;

    QPointer<QProcess> m_hid;
    QPointer<QProcess> m_closing;   // a stopped session's shell, still restoring the tablet setting
    bool m_hidReady = false;
    QByteArray m_hidOutput;
    QTcpSocket* m_socket = nullptr;
    QTimer* m_retry = nullptr;
    QTimer* m_motion = nullptr;     // paces mouse motion: at most one report per tick
    std::unique_ptr<tab::Session> m_session;
    tab::Metrics m_metrics;
    int m_density = 0;
    QString m_lastStatus;
};
