/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdint>

// Pure logic behind TabBridge: speaks the InputLeap client protocol (1.6, plain TCP) to the local
// server as one extra screen and turns what it hears into HID reports for an Android tablet.
// The reports are JSON lines for Android's stock `hid` shell tool (`adb shell hid -`), which
// registers real virtual USB devices, so DeX sees a genuine mouse and keyboard.
namespace tab {

// `hid -` device ids
constexpr int kMouseId = 1;
constexpr int kKeyboardId = 2;

// Lines that register the virtual mouse and keyboard; send them first.
QStringList registerLines();
// Names the devices get on the tablet (dumpsys input lists them under these).
QString mouseDeviceName();

// USB HID keyboard usage (page 0x07) for a Mac server key button (Mac virtual keycode + 1),
// 0 when there is none.
std::uint8_t macButtonToHid(std::uint16_t button);

// What the tablet reported about our virtual mouse once it exists.
struct Metrics {
    int width = 0;        // logical display size in pixels, current rotation
    int height = 0;
    double gain = 0;      // pixels per HID count (Android's pointer speed, acceleration off)
    bool linear = false;  // a single curve segment: acceleration really is off
};
// Parses `dumpsys input` for the device named `deviceName`. False if it is not listed yet.
bool parseMetrics(const QString& dumpsys, const QString& deviceName, Metrics* out);
// Parses `wm density` ("Physical density: 320", maybe followed by "Override density: N").
int parseDensity(const QString& text);

class Session {
public:
    // width/height: the screen size declared to the server, in server pixels (Mac points).
    // countsPerPixel: HID counts per server pixel, so the tablet cursor tracks the server's idea
    // of it exactly while Android's acceleration is off.
    Session(const QString& screenName, int width, int height, double countsPerPixel);

    // Bytes from the server in, bytes to send back out. Call takeHidLines() afterwards.
    QByteArray feed(const QByteArray& data);
    // Pending HID report lines (queued mouse motion is coalesced into one report here).
    QStringList takeHidLines();
    // Lifts every key and button the tablet thinks are down.
    void releaseAll();

    bool failed() const { return !m_error.isEmpty(); }
    QString error() const { return m_error; }     // Korean
    bool accepted() const { return m_accepted; }  // the server sent options = it took this screen

private:
    void handle(const QByteArray& msg, QByteArray* reply);
    void enter(int x, int y);
    void moveTo(int x, int y);
    void relativeMove(int dx, int dy);
    void mouseButton(int id, bool down);
    void wheel(int dx, int dy);
    void keyDown(std::uint32_t id, std::uint16_t button);
    void keyUp(std::uint16_t button);
    void setOptions(const QByteArray& msg);
    void flushMotion();
    void sendMouse(int dx, int dy, int wheel, int pan);
    void sendKeyboard();
    void sendConsumer(std::uint16_t usage);
    std::uint8_t remapModifier(std::uint8_t usage) const;

    QString m_name;
    int m_width;
    int m_height;
    double m_countsPerPixel;

    QByteArray m_in;
    QStringList m_out;
    QString m_error;
    bool m_hello = false;
    bool m_accepted = false;

    int m_x = 0;        // server pixels
    int m_y = 0;
    int m_cx = 0;       // HID counts from the corner the cursor was last pushed into
    int m_cy = 0;
    int m_pendingX = 0; // counts not yet sent
    int m_pendingY = 0;
    double m_relX = 0;  // fractions of a count left over from relative moves
    double m_relY = 0;
    int m_wheelX = 0;   // wheel remainders, 120 = one notch
    int m_wheelY = 0;
    std::uint8_t m_buttons = 0;
    std::uint8_t m_modifiers = 0;
    QByteArray m_keys;                          // pressed non-modifier usages, at most 6
    QHash<std::uint16_t, std::uint8_t> m_down;  // server button -> usage it pressed
    QHash<int, int> m_modifierMap;              // KeyModifierID -> KeyModifierID, from DSOP
};

} // namespace tab
