/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "TabBridgeCore.h"

#include "inputleap/key_types.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace tab {

namespace {

// Wire facts from src/lib/inputleap/protocol_types.{h,cpp} and ProtocolUtil (all big-endian).
constexpr std::int16_t kMajor = 1;
constexpr std::int16_t kMinor = 6;
constexpr int kMaxPacket = 4 * 1024 * 1024;
constexpr std::uint16_t kLanguageSwitch = 0x029D;   // consumer "AC Next Keyboard Layout Select"

// boot-style relative mouse: 5 buttons, int16 X/Y, wheel, AC Pan. Report: 7 bytes, no report id.
const std::initializer_list<int> kMouseDescriptor = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x16, 0x01, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x02, 0x81, 0x06,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
    0x05, 0x0C, 0x0A, 0x38, 0x02, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
    0xC0, 0xC0};

// report id 1: boot keyboard (modifiers, reserved, 6 keys); report id 2: one 16-bit consumer usage
const std::initializer_list<int> kKeyboardDescriptor = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00, 0x81, 0x00,
    0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03,
    0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xC0};

// Mac virtual keycode (kVK_*, HIToolbox/Events.h) -> USB HID usage, US ANSI.
const std::uint8_t kMacVkToHid[128] = {
    /*00*/ 0x04, 0x16, 0x07, 0x09, 0x0B, 0x0A, 0x1D, 0x1B, 0x06, 0x19, 0x64, 0x05, 0x14, 0x1A, 0x08, 0x15,
    /*10*/ 0x1C, 0x17, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x22, 0x2E, 0x26, 0x24, 0x2D, 0x25, 0x27, 0x30, 0x12,
    /*20*/ 0x18, 0x2F, 0x0C, 0x13, 0x28, 0x0F, 0x0D, 0x34, 0x0E, 0x33, 0x31, 0x36, 0x38, 0x11, 0x10, 0x37,
    /*30*/ 0x2B, 0x2C, 0x35, 0x2A, 0x00, 0x29, 0xE7, 0xE3, 0xE1, 0x39, 0xE2, 0xE0, 0xE5, 0xE6, 0xE4, 0x00,
    /*40*/ 0x6C, 0x63, 0x00, 0x55, 0x00, 0x57, 0x00, 0x53, 0x80, 0x81, 0x7F, 0x54, 0x58, 0x00, 0x56, 0x6D,
    /*50*/ 0x6E, 0x67, 0x62, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x6F, 0x60, 0x61, 0x89, 0x87, 0x85,
    /*60*/ 0x3E, 0x3F, 0x40, 0x3C, 0x41, 0x42, 0x91, 0x44, 0x90, 0x68, 0x6B, 0x69, 0x00, 0x43, 0x65, 0x45,
    /*70*/ 0x00, 0x6A, 0x49, 0x4A, 0x4B, 0x4C, 0x3D, 0x4D, 0x3B, 0x4E, 0x3A, 0x50, 0x4F, 0x51, 0x52, 0x00,
};

QString hidLine(int id, const QString& command, const QString& body)
{
    return QStringLiteral("{\"id\":%1,\"command\":\"%2\",%3}").arg(id).arg(command, body);
}

QString numbers(std::initializer_list<int> values)
{
    QStringList parts;
    for (int v : values)
        parts << QString::number(v);
    return parts.join(QLatin1Char(','));
}

QString numbers(const QByteArray& bytes)
{
    QStringList parts;
    for (char c : bytes)
        parts << QString::number(static_cast<std::uint8_t>(c));
    return parts.join(QLatin1Char(','));
}

std::uint16_t u16(const QByteArray& m, int at)
{
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(m[at]) << 8) | static_cast<std::uint8_t>(m[at + 1]));
}

std::int16_t i16(const QByteArray& m, int at) { return static_cast<std::int16_t>(u16(m, at)); }

std::uint32_t u32(const QByteArray& m, int at)
{
    return (static_cast<std::uint32_t>(u16(m, at)) << 16) | u16(m, at + 2);
}

void put16(QByteArray* out, int v)
{
    out->append(static_cast<char>((v >> 8) & 0xFF));
    out->append(static_cast<char>(v & 0xFF));
}

void put32(QByteArray* out, std::uint32_t v)
{
    put16(out, static_cast<int>(v >> 16));
    put16(out, static_cast<int>(v & 0xFFFF));
}

void appendFrame(QByteArray* out, const QByteArray& payload)
{
    put32(out, static_cast<std::uint32_t>(payload.size()));
    out->append(payload);
}

constexpr std::uint32_t optionCode(const char (&s)[5])
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(s[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(s[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(s[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(s[3]));
}

bool isModifierUsage(std::uint8_t usage) { return usage >= 0xE0 && usage <= 0xE7; }

} // namespace

QStringList registerLines()
{
    const QString common = QStringLiteral("\"bus\":\"usb\",\"vid\":4617,\"pid\":%1,\"name\":\"%2\",\"descriptor\":[%3]");
    return {
        hidLine(kMouseId, QStringLiteral("register"),
                common.arg(QString::number(0x4C01), mouseDeviceName(), numbers(kMouseDescriptor))),
        hidLine(kKeyboardId, QStringLiteral("register"),
                common.arg(QString::number(0x4C02), QStringLiteral("InputLeafPlus Keyboard"), numbers(kKeyboardDescriptor))),
    };
}

QString mouseDeviceName() { return QStringLiteral("InputLeafPlus Mouse"); }

std::uint8_t macButtonToHid(std::uint16_t button)
{
    // the Mac server sends kVK + 1 (OSXKeyState::KeyButtonOffset); 0 means "no physical key"
    if (button == 0 || button > 128)
        return 0;
    return kMacVkToHid[button - 1];
}

bool parseMetrics(const QString& dumpsys, const QString& deviceName, Metrics* out)
{
    // "Input Reader State" lists every device as "  Device <n>: <name>" followed by its mappers
    const QRegularExpression header(QStringLiteral("\\n\\s*Device \\d+: %1\\n").arg(QRegularExpression::escape(deviceName)));
    const QRegularExpressionMatch start = header.match(dumpsys);
    if (!start.hasMatch())
        return false;
    int end = dumpsys.indexOf(QRegularExpression(QStringLiteral("\\n\\s*Device \\d+: ")), start.capturedEnd());
    const QString section = dumpsys.mid(start.capturedEnd(), end < 0 ? -1 : end - start.capturedEnd());

    const auto range = [&section](QChar axis) {
        const QRegularExpressionMatch m = QRegularExpression(
            QStringLiteral("\\b%1: source=MOUSE, min=[-\\d.]+, max=([\\d.]+)").arg(axis)).match(section);
        return m.hasMatch() ? qRound(m.captured(1).toDouble()) + 1 : 0;
    };
    const int curve = section.indexOf(QStringLiteral("Acceleration Curve"));
    if (curve < 0)
        return false;
    QRegularExpressionMatchIterator segments = QRegularExpression(
        QStringLiteral("^\\s*\\{[^,]+, ([\\d.]+), [-\\d.]+\\}\\s*$"), QRegularExpression::MultilineOption)
        .globalMatch(section.mid(section.indexOf(QLatin1Char('\n'), curve)));
    int count = 0;
    double gain = 0;
    while (segments.hasNext()) {
        const QRegularExpressionMatch m = segments.next();
        if (count++ == 0)
            gain = m.captured(1).toDouble();
    }

    Metrics metrics{range(QLatin1Char('X')), range(QLatin1Char('Y')), gain, count == 1};
    if (metrics.width <= 0 || metrics.height <= 0 || metrics.gain <= 0)
        return false;
    *out = metrics;
    return true;
}

int parseDensity(const QString& text)
{
    int density = 0;
    QRegularExpressionMatchIterator it = QRegularExpression(QStringLiteral("density: (\\d+)")).globalMatch(text);
    while (it.hasNext())
        density = it.next().captured(1).toInt();   // the override, when present, comes last
    return density;
}

Session::Session(const QString& screenName, int width, int height, double countsPerPixel) :
    m_name(screenName),
    m_width(std::clamp(width, 1, 32767)),
    m_height(std::clamp(height, 1, 32767)),
    m_countsPerPixel(countsPerPixel)
{
}

QByteArray Session::feed(const QByteArray& data)
{
    QByteArray reply;
    m_in.append(data);
    while (!failed() && m_in.size() >= 4) {
        const std::uint32_t size = u32(m_in, 0);
        if (size > static_cast<std::uint32_t>(kMaxPacket)) {
            m_error = QStringLiteral("서버에서 잘못된 데이터가 왔습니다.");
            break;
        }
        if (static_cast<std::uint32_t>(m_in.size()) < 4 + size)
            break;
        const QByteArray msg = m_in.mid(4, static_cast<int>(size));
        m_in.remove(0, static_cast<int>(4 + size));
        handle(msg, &reply);
    }
    return reply;
}

void Session::handle(const QByteArray& msg, QByteArray* reply)
{
    if (!m_hello) {
        if (!msg.startsWith("Barrier") || msg.size() < 11) {
            m_error = QStringLiteral("InputLeafPlus 서버가 아닌 곳에 연결됐습니다.");
            return;
        }
        m_hello = true;
        QByteArray back("Barrier");
        put16(&back, kMajor);
        put16(&back, kMinor);
        const QByteArray name = m_name.toUtf8();
        put32(&back, static_cast<std::uint32_t>(name.size()));
        back.append(name);
        appendFrame(reply, back);
        return;
    }
    if (msg.size() < 4)
        return;
    const QByteArray code = msg.left(4);
    const auto has = [&msg](int bytes) { return msg.size() >= 4 + bytes; };

    if (code == "QINF") {
        QByteArray info("DINF");
        for (int v : {0, 0, m_width, m_height, 0, m_width / 2, m_height / 2})
            put16(&info, v);
        appendFrame(reply, info);
    } else if (code == "CALV") {
        appendFrame(reply, QByteArray("CALV"));
    } else if (code == "DSOP") {
        setOptions(msg);
        m_accepted = true;
    } else if (code == "CROP") {
        m_modifierMap.clear();
    } else if (code == "CINN" && has(4)) {
        enter(i16(msg, 4), i16(msg, 6));
    } else if (code == "COUT") {
        releaseAll();
    } else if (code == "DMMV" && has(4)) {
        moveTo(i16(msg, 4), i16(msg, 6));
    } else if (code == "DMRM" && has(4)) {
        relativeMove(i16(msg, 4), i16(msg, 6));   // game mode, while locked to this screen
    } else if ((code == "DMDN" || code == "DMUP") && has(1)) {
        mouseButton(static_cast<std::uint8_t>(msg[4]), code == "DMDN");
    } else if (code == "DMWM" && has(4)) {
        wheel(i16(msg, 4), i16(msg, 6));
    } else if (code == "DKDN" && has(6)) {
        keyDown(u16(msg, 4), u16(msg, 8));
    } else if (code == "DKUP" && has(6)) {
        keyUp(u16(msg, 8));
    } else if (code == "EICV") {
        m_error = QStringLiteral("서버와 프로토콜 버전이 맞지 않습니다.");
    } else if (code == "EBSY") {
        m_error = QStringLiteral("'%1' 이름의 화면이 이미 서버에 연결돼 있습니다.").arg(m_name);
    } else if (code == "EUNK") {
        m_error = QStringLiteral("서버 화면 배치에 '%1' 화면이 없습니다.").arg(m_name);
    } else if (code == "EBAD") {
        m_error = QStringLiteral("서버가 연결을 거부했습니다.");
    } else if (code == "CBYE") {
        m_error = QStringLiteral("서버가 연결을 닫았습니다.");
    }
    // everything else (CIAK, CNOP, DKRP: the tablet repeats held keys itself, clipboard and
    // drag-and-drop data, screensaver) is ignored on purpose
}

void Session::setOptions(const QByteArray& msg)
{
    static const QHash<std::uint32_t, int> modifierOptions = {
        {optionCode("MMFS"), kKeyModifierIDShift}, {optionCode("MMFC"), kKeyModifierIDControl},
        {optionCode("MMFA"), kKeyModifierIDAlt},   {optionCode("MMFG"), kKeyModifierIDAltGr},
        {optionCode("MMFM"), kKeyModifierIDMeta},  {optionCode("MMFR"), kKeyModifierIDSuper},
    };
    if (msg.size() < 8)
        return;
    const std::uint32_t count = u32(msg, 4);
    for (std::uint32_t i = 0; i + 1 < count && 8 + 4 * (i + 2) <= static_cast<std::uint32_t>(msg.size()); i += 2) {
        const auto option = modifierOptions.constFind(u32(msg, static_cast<int>(8 + 4 * i)));
        if (option != modifierOptions.constEnd())
            m_modifierMap[*option] = static_cast<int>(u32(msg, static_cast<int>(8 + 4 * (i + 1))));
    }
}

void Session::enter(int x, int y)
{
    // a relative mouse can't jump, so push it into the top-left corner (Android clamps there)
    // and count from that known origin
    flushMotion();
    sendMouse(-32767, -32767, 0, 0);
    sendMouse(-32767, -32767, 0, 0);
    m_cx = m_cy = 0;
    moveTo(x, y);
}

void Session::moveTo(int x, int y)
{
    m_x = std::clamp(x, 0, m_width - 1);
    m_y = std::clamp(y, 0, m_height - 1);
    const int cx = static_cast<int>(std::lround(m_x * m_countsPerPixel));
    const int cy = static_cast<int>(std::lround(m_y * m_countsPerPixel));
    m_pendingX += cx - m_cx;
    m_pendingY += cy - m_cy;
    m_cx = cx;
    m_cy = cy;
}

void Session::relativeMove(int dx, int dy)
{
    // raw deltas, unclamped: a game holding the pointer keeps turning past the screen edge.
    // shortcut: the cursor then no longer matches m_x/m_y; the next enter re-homes it.
    m_relX += dx * m_countsPerPixel;
    m_relY += dy * m_countsPerPixel;
    const int cx = static_cast<int>(std::lround(m_relX));
    const int cy = static_cast<int>(std::lround(m_relY));
    m_relX -= cx;
    m_relY -= cy;
    m_pendingX += cx;
    m_pendingY += cy;
}

void Session::mouseButton(int id, bool down)
{
    // InputLeap: 1 left, 2 middle, 3 right, 4/5 extra; HID bits: left, right, middle, back, forward
    static const std::uint8_t bits[] = {0, 0x01, 0x04, 0x02, 0x08, 0x10};
    if (id < 1 || id > 5)
        return;
    flushMotion();
    if (down)
        m_buttons |= bits[id];
    else
        m_buttons &= static_cast<std::uint8_t>(~bits[id]);
    sendMouse(0, 0, 0, 0);
}

void Session::wheel(int dx, int dy)
{
    // 120 = one notch; the Mac sends finer, accelerated steps, so keep the remainder
    m_wheelX += dx;
    m_wheelY += dy;
    const int notchesX = std::clamp(m_wheelX / 120, -127, 127);
    const int notchesY = std::clamp(m_wheelY / 120, -127, 127);
    m_wheelX -= notchesX * 120;
    m_wheelY -= notchesY * 120;
    if (notchesX == 0 && notchesY == 0)
        return;
    flushMotion();
    sendMouse(0, 0, notchesY, notchesX);
}

void Session::keyDown(std::uint32_t id, std::uint16_t button)
{
    // the Mac's language keys (Caps Lock = Karabiner F18 sent as Caps Lock, right Command = Hangul)
    // become Android's language switch; Samsung Keyboard toggles 한/영 on it.
    // shortcut: a real Caps Lock key also switches the language, add a setting if anyone needs Caps.
    if (id == kKeyHangul || id == kKeyCapsLock) {
        sendConsumer(kLanguageSwitch);
        return;
    }
    // shortcut: buttons are Mac keycodes; after a role swap (Windows as server) they'd be Windows
    // scancodes and this table is wrong, so the bridge only runs on a Mac server.
    std::uint8_t usage = macButtonToHid(button);
    if (usage == 0 || m_down.contains(button))
        return;
    if (isModifierUsage(usage)) {
        usage = remapModifier(usage);
        if (usage == 0)
            return;
        m_modifiers |= static_cast<std::uint8_t>(1 << (usage - 0xE0));
    } else {
        if (m_keys.size() >= 6)
            return;
        m_keys.append(static_cast<char>(usage));
    }
    m_down.insert(button, usage);
    sendKeyboard();
}

void Session::keyUp(std::uint16_t button)
{
    auto it = m_down.constFind(button);
    const std::uint8_t raw = macButtonToHid(button);
    if (it == m_down.constEnd() && isModifierUsage(raw)) {
        // the Mac reports a modifier kind, not a side: left Shift down, right Shift down, left up,
        // right up arrives as "left down, right up", so release whichever side of that kind is held
        for (it = m_down.constBegin(); it != m_down.constEnd(); ++it) {
            const std::uint8_t held = macButtonToHid(it.key());
            if (isModifierUsage(held) && (held & 0x03) == (raw & 0x03))
                break;
        }
    }
    if (it == m_down.constEnd())
        return;
    const std::uint8_t usage = *it;
    m_down.erase(it);
    if (isModifierUsage(usage)) {
        // two Mac keys can be remapped onto one modifier; keep it while either is held
        if (std::find(m_down.cbegin(), m_down.cend(), usage) == m_down.cend())
            m_modifiers &= static_cast<std::uint8_t>(~(1 << (usage - 0xE0)));
    } else {
        m_keys.remove(m_keys.indexOf(static_cast<char>(usage)), 1);
    }
    sendKeyboard();
}

std::uint8_t Session::remapModifier(std::uint8_t usage) const
{
    // the Mac's Command is Super (OSXKeyState); per-screen "alt = super" style options arrive in
    // DSOP and are applied by the client, exactly like ServerProxy::translateKey does
    const bool right = usage >= 0xE4;
    int id = kKeyModifierIDNull;
    switch (usage & 0x03) {
    case 0: id = kKeyModifierIDControl; break;
    case 1: id = kKeyModifierIDShift; break;
    case 2: id = kKeyModifierIDAlt; break;
    case 3: id = kKeyModifierIDSuper; break;
    }
    switch (m_modifierMap.value(id, id)) {
    case kKeyModifierIDShift: return right ? 0xE5 : 0xE1;
    case kKeyModifierIDControl: return right ? 0xE4 : 0xE0;
    case kKeyModifierIDAlt: return right ? 0xE6 : 0xE2;
    case kKeyModifierIDAltGr: return 0xE6;
    case kKeyModifierIDMeta:
    case kKeyModifierIDSuper: return right ? 0xE7 : 0xE3;
    default: return 0;   // "none": the key is switched off on this screen
    }
}

void Session::releaseAll()
{
    flushMotion();
    if (m_buttons != 0) {
        m_buttons = 0;
        sendMouse(0, 0, 0, 0);
    }
    if (m_modifiers != 0 || !m_keys.isEmpty()) {
        m_modifiers = 0;
        m_keys.clear();
        sendKeyboard();
    }
    m_down.clear();
    m_wheelX = m_wheelY = 0;
}

QStringList Session::takeHidLines(bool withMotion)
{
    if (withMotion)
        flushMotion();
    QStringList lines;
    lines.swap(m_out);
    return lines;
}

void Session::flushMotion()
{
    while (m_pendingX != 0 || m_pendingY != 0) {
        const int dx = std::clamp(m_pendingX, -32767, 32767);
        const int dy = std::clamp(m_pendingY, -32767, 32767);
        m_pendingX -= dx;
        m_pendingY -= dy;
        sendMouse(dx, dy, 0, 0);
    }
}

void Session::sendMouse(int dx, int dy, int wheel, int pan)
{
    QByteArray r;
    r.append(static_cast<char>(m_buttons));
    r.append(static_cast<char>(dx & 0xFF));
    r.append(static_cast<char>((dx >> 8) & 0xFF));
    r.append(static_cast<char>(dy & 0xFF));
    r.append(static_cast<char>((dy >> 8) & 0xFF));
    r.append(static_cast<char>(wheel));
    r.append(static_cast<char>(pan));
    m_out << hidLine(kMouseId, QStringLiteral("report"), QStringLiteral("\"report\":[%1]").arg(numbers(r)));
    // Wireless adb delivers lines in bursts; written to the tablet at once, 150+ reports overflow the
    // kernel's input buffer and Android drops that motion (the cursor then stops short of the edge).
    // `hid` holds the next report of this device until this long after the previous one.
    // shortcut: keyboard reports are not held, so in a burst a key can overtake a click.
    m_out << hidLine(kMouseId, QStringLiteral("delay"), QStringLiteral("\"duration\":%1").arg(kMouseGapMs));
}

void Session::sendKeyboard()
{
    QByteArray r;
    r.append(static_cast<char>(1));
    r.append(static_cast<char>(m_modifiers));
    r.append('\0');
    r.append(m_keys);
    r.append(QByteArray(6 - m_keys.size(), '\0'));
    flushMotion();
    m_out << hidLine(kKeyboardId, QStringLiteral("report"), QStringLiteral("\"report\":[%1]").arg(numbers(r)));
}

void Session::sendConsumer(std::uint16_t usage)
{
    flushMotion();
    m_out << hidLine(kKeyboardId, QStringLiteral("report"),
                     QStringLiteral("\"report\":[2,%1,%2]").arg(usage & 0xFF).arg(usage >> 8));
    m_out << hidLine(kKeyboardId, QStringLiteral("report"), QStringLiteral("\"report\":[2,0,0]"));
}

} // namespace tab
