/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "../src/TabBridgeCore.h"
#include <gtest/gtest.h>

#include <initializer_list>

namespace {

QByteArray be16(int v)
{
    QByteArray b;
    b.append(static_cast<char>((v >> 8) & 0xFF));
    b.append(static_cast<char>(v & 0xFF));
    return b;
}

QByteArray be32(std::uint32_t v) { return be16(static_cast<int>(v >> 16)) + be16(static_cast<int>(v & 0xFFFF)); }

QByteArray frame(const QByteArray& payload) { return be32(static_cast<std::uint32_t>(payload.size())) + payload; }

QByteArray msg(const char* code, std::initializer_list<int> shorts = {})
{
    QByteArray m(code);
    for (int v : shorts)
        m += be16(v);
    return frame(m);
}

QString mouse(std::initializer_list<int> bytes)
{
    QStringList parts;
    for (int b : bytes)
        parts << QString::number(b);
    return QStringLiteral("{\"id\":1,\"command\":\"report\",\"report\":[%1]}").arg(parts.join(','));
}

QString keyboard(std::initializer_list<int> bytes)
{
    QStringList parts;
    for (int b : bytes)
        parts << QString::number(b);
    return QStringLiteral("{\"id\":2,\"command\":\"report\",\"report\":[%1]}").arg(parts.join(','));
}

// a session that went through hello and got (optionally) some options
tab::Session connected(double countsPerPixel = 0.8, const QByteArray& options = be32(0))
{
    tab::Session s(QStringLiteral("GalaxyTab"), 1480, 924, countsPerPixel);
    s.feed(frame(QByteArray("Barrier") + be16(1) + be16(6)));
    s.feed(frame(QByteArray("DSOP") + options));
    s.takeHidLines();
    return s;
}

} // namespace

TEST(TabBridgeCore, handshake_keepalive_and_info)
{
    tab::Session s(QStringLiteral("GalaxyTab"), 1480, 924, 0.8);
    // the hello may arrive in pieces
    const QByteArray hello = frame(QByteArray("Barrier") + be16(1) + be16(6));
    EXPECT_TRUE(s.feed(hello.left(5)).isEmpty());
    EXPECT_EQ(s.feed(hello.mid(5)), frame(QByteArray("Barrier") + be16(1) + be16(6) + be32(9) + "GalaxyTab"));

    EXPECT_EQ(s.feed(msg("QINF")), frame(QByteArray("DINF") + be16(0) + be16(0) + be16(1480) + be16(924) + be16(0) +
                                         be16(740) + be16(462)));
    EXPECT_EQ(s.feed(msg("CALV")), msg("CALV"));
    EXPECT_FALSE(s.accepted());
    EXPECT_TRUE(s.feed(frame(QByteArray("DSOP") + be32(0))).isEmpty());
    EXPECT_TRUE(s.accepted());
    EXPECT_FALSE(s.failed());
}

TEST(TabBridgeCore, rejects_non_server_and_unknown_name)
{
    tab::Session wrong(QStringLiteral("GalaxyTab"), 100, 100, 1);
    wrong.feed(frame("HTTP/1.1 200"));
    EXPECT_TRUE(wrong.failed());

    tab::Session s = connected();
    s.feed(msg("EUNK"));
    EXPECT_TRUE(s.failed());
}

TEST(TabBridgeCore, key_down_up_by_mac_button)
{
    tab::Session s = connected();
    s.feed(msg("DKDN", {'a', 0, 0x00 + 1}));   // kVK_ANSI_A = 0, sent as button 1
    s.feed(msg("DKUP", {0, 0, 1}));            // key up carries no id
    const QStringList lines = s.takeHidLines();
    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], keyboard({1, 0, 0, 0x04, 0, 0, 0, 0, 0}));
    EXPECT_EQ(lines[1], keyboard({1, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(TabBridgeCore, modifier_options_remap_command)
{
    // "super = alt" for this screen: MMFR -> kKeyModifierIDAlt (3)
    tab::Session s = connected(0.8, be32(2) + QByteArray("MMFR") + be32(3));
    s.feed(msg("DKDN", {0xEFEB, 0, 0x37 + 1}));   // left Command (Super_L)
    s.feed(msg("DKDN", {0xEFE3, 0, 0x3B + 1}));   // left Control stays Control
    EXPECT_EQ(s.takeHidLines(), QStringList({keyboard({1, 0x04, 0, 0, 0, 0, 0, 0, 0}),
                                             keyboard({1, 0x05, 0, 0, 0, 0, 0, 0, 0})}));
    s.feed(msg("COUT"));   // leaving the tablet lifts everything
    EXPECT_EQ(s.takeHidLines(), QStringList({keyboard({1, 0, 0, 0, 0, 0, 0, 0, 0})}));
}

TEST(TabBridgeCore, language_keys_send_language_switch)
{
    tab::Session s = connected();
    const QStringList expected = {keyboard({2, 0x9D, 0x02}), keyboard({2, 0, 0})};
    s.feed(msg("DKDN", {0xEF31, 0, 0}));          // right Command = kKeyHangul, no button
    EXPECT_EQ(s.takeHidLines(), expected);
    s.feed(msg("DKDN", {0xEFE5, 0x1000, 0x4F + 1}));   // Caps Lock (Karabiner F18)
    s.feed(msg("DKUP", {0, 0x1000, 0x4F + 1}));
    EXPECT_EQ(s.takeHidLines(), expected);
}

TEST(TabBridgeCore, absolute_moves_become_counts_from_the_corner)
{
    tab::Session s = connected(0.8);
    s.feed(msg("CINN", {10, 20, 0, 0, 0}));   // x, y, seq (4 bytes), mask
    EXPECT_EQ(s.takeHidLines(), QStringList({mouse({0, 1, 128, 1, 128, 0, 0}), mouse({0, 1, 128, 1, 128, 0, 0}),
                                             mouse({0, 8, 0, 16, 0, 0, 0})}));
    // two moves in one read are coalesced into one report: 11 -> 8.8 -> 9, 12 -> 9.6 -> 10
    s.feed(msg("DMMV", {11, 20}) + msg("DMMV", {12, 20}));
    EXPECT_EQ(s.takeHidLines(), QStringList({mouse({0, 2, 0, 0, 0, 0, 0})}));
    // a click flushes the motion first, and the position is clamped to the declared screen
    s.feed(msg("DMMV", {5000, 20}));
    s.feed(frame(QByteArray("DMDN") + QByteArray(1, '\x03')));
    const QStringList lines = s.takeHidLines();
    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], mouse({0, (1183 - 10) & 0xFF, (1183 - 10) >> 8, 0, 0, 0, 0}));   // lround(1479 * 0.8) = 1183
    EXPECT_EQ(lines[1], mouse({2, 0, 0, 0, 0, 0, 0}));                                  // right button
}

TEST(TabBridgeCore, wheel_keeps_remainder)
{
    tab::Session s = connected();
    s.feed(msg("DMWM", {0, 60}));
    EXPECT_TRUE(s.takeHidLines().isEmpty());
    s.feed(msg("DMWM", {0, 60}));
    EXPECT_EQ(s.takeHidLines(), QStringList({mouse({0, 0, 0, 0, 0, 1, 0})}));
}

TEST(TabBridgeCore, parses_dumpsys_metrics)
{
    const QString dumpsys = QStringLiteral(
        "Input Reader State (Nums of device: 9):\n"
        "  Device 27: Samsung Virtual Mouse\n"
        "    Motion Ranges:\n"
        "      X: source=MOUSE, min=0.000, max=99.000, flat=0.000, fuzz=0.000, resolution=0.000\n"
        "  Device 60: InputLeafPlus Mouse\n"
        "    Motion Ranges:\n"
        "      X: source=MOUSE, min=0.000, max=2959.000, flat=0.000, fuzz=0.000, resolution=0.000\n"
        "      Y: source=MOUSE, min=0.000, max=1847.000, flat=0.000, fuzz=0.000, resolution=0.000\n"
        "    Cursor Input Mapper:\n"
        "      Acceleration Curve {maxPointerSpeedMmPerS, baseGain, reciprocal}:\n"
        "        {inf, 2.449920, 0.000000}\n"
        "      VWheelScale: 1.000\n"
        "  Device 61: InputLeafPlus Keyboard\n"
        "      Acceleration Curve {maxPointerSpeedMmPerS, baseGain, reciprocal}:\n"
        "        {inf, 9.000000, 0.000000}\n");
    tab::Metrics m;
    ASSERT_TRUE(tab::parseMetrics(dumpsys, tab::mouseDeviceName(), &m));
    EXPECT_EQ(m.width, 2960);
    EXPECT_EQ(m.height, 1848);
    EXPECT_DOUBLE_EQ(m.gain, 2.44992);
    EXPECT_TRUE(m.linear);

    QString accelerated = dumpsys;
    accelerated.replace(QStringLiteral("        {inf, 2.449920, 0.000000}\n"),
                        QStringLiteral("        {32.002000, 2.449920, 0.000000}\n        {inf, 11.550720, -850.603008}\n"));
    ASSERT_TRUE(tab::parseMetrics(accelerated, tab::mouseDeviceName(), &m));
    EXPECT_FALSE(m.linear);
    EXPECT_FALSE(tab::parseMetrics(dumpsys, QStringLiteral("Missing Mouse"), &m));

    EXPECT_EQ(tab::parseDensity(QStringLiteral("Physical density: 320\nOverride density: 280\n")), 280);
    EXPECT_EQ(tab::parseDensity(QStringLiteral("Physical density: 320\n")), 320);
}

TEST(TabBridgeCore, mac_keycode_table)
{
    EXPECT_EQ(tab::macButtonToHid(0), 0);        // no physical key
    EXPECT_EQ(tab::macButtonToHid(0x24 + 1), 0x28);   // Return
    EXPECT_EQ(tab::macButtonToHid(0x7A + 1), 0x3A);   // F1
    EXPECT_EQ(tab::macButtonToHid(0x7E + 1), 0x52);   // Up
    EXPECT_EQ(tab::macButtonToHid(0x3F + 1), 0);      // Fn has no HID usage
    EXPECT_EQ(tab::macButtonToHid(200), 0);
}

TEST(TabBridgeCore, modifier_released_from_the_other_side)
{
    // left Shift down, right Shift down, left up, right up reaches the client as "left down, right up"
    tab::Session s = connected();
    s.feed(msg("DKDN", {0xEFE1, 0, 0x38 + 1}));   // kVK_Shift
    s.feed(msg("DKUP", {0xEFE2, 0, 0x3C + 1}));   // kVK_RightShift
    EXPECT_EQ(s.takeHidLines(), QStringList({keyboard({1, 0x02, 0, 0, 0, 0, 0, 0, 0}),
                                             keyboard({1, 0, 0, 0, 0, 0, 0, 0, 0})}));
}

TEST(TabBridgeCore, game_mode_relative_moves_are_not_clamped)
{
    tab::Session s = connected(0.5);
    s.feed(msg("CINN", {1479, 0, 0, 0, 0}));   // at the right edge
    s.takeHidLines();
    s.feed(msg("DMRM", {3, 0}) + msg("DMRM", {3, 0}));   // 1.5 + 1.5 counts, past the edge
    EXPECT_EQ(s.takeHidLines(), QStringList({mouse({0, 3, 0, 0, 0, 0, 0})}));
}
