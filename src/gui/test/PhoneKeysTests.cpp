/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "../src/PhoneKeys.h"
#include <gtest/gtest.h>

#include <QString>
#include <QStringList>

namespace {

// Renders strokes as "g k s"; a '+' prefix means Shift was held for that key.
QString render(const QString& text, int* skipped = nullptr)
{
    QStringList parts;
    for (const phone::KeyStroke& s : phone::textToStrokes(text, skipped))
        parts << (s.shift ? "+" : "") + phone::keyName(s.key);
    return parts.join(' ');
}

QString hangul(std::initializer_list<ushort> codes)
{
    QString s;
    for (ushort c : codes)
        s += QChar(c);
    return s;
}

} // namespace

TEST(PhoneKeys, ascii_text)
{
    EXPECT_EQ(render("Hello World!"), "+h e l l o Space +w o r l d +1");
    EXPECT_EQ(render("abc xyz 0189"), "a b c Space x y z Space 0 1 8 9");
}

TEST(PhoneKeys, ascii_symbols_unshifted_and_shifted)
{
    EXPECT_EQ(render("-=[]\\;',./`"), "- = [ ] \\ ; ' , . / `");
    EXPECT_EQ(render("_+{}|:\"<>?~"), "+- += +[ +] +\\ +; +' +, +. +/ +`");
    EXPECT_EQ(render(")!@#$%^&*("), "+0 +1 +2 +3 +4 +5 +6 +7 +8 +9");
}

TEST(PhoneKeys, whitespace_and_control)
{
    int skipped = -1;
    EXPECT_EQ(render("a\nb\tc", &skipped), "a Enter b Tab c");
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(render("a\r\nb", &skipped), "a Enter b");   // CRLF: '\r' is dropped silently
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(render(QString("a") + QChar(1) + QChar(0x7F) + "b", &skipped), "a b");
    EXPECT_EQ(skipped, 2);
}

TEST(PhoneKeys, skipped_count)
{
    int skipped = -1;
    // e-acute, an emoji (surrogate pair counts once), a CJK ideograph
    const QString text = QString("x") + QChar(0xE9) + QString::fromUcs4(U"\U0001F600") + QChar(0x4E2D) + "y";
    EXPECT_EQ(render(text, &skipped), "x y");
    EXPECT_EQ(skipped, 3);
    skipped = -1;
    phone::textToStrokes(QString(), &skipped);
    EXPECT_EQ(skipped, 0);
    EXPECT_TRUE(phone::textToStrokes(QString(), nullptr).isEmpty());
    EXPECT_TRUE(phone::textToStrokes(QString(QChar(0xD83D))).isEmpty());   // lone surrogate
}

TEST(PhoneKeys, hangul_syllables)
{
    EXPECT_EQ(render(hangul({0xD55C, 0xAE00})), "g k s r m f");          // 한글
    EXPECT_EQ(render(hangul({0xAC12})), "r k q t");                      // 값 (compound final)
    EXPECT_EQ(render(hangul({0xC758})), "d m l");                        // 의
    EXPECT_EQ(render(hangul({0xC65C})), "d h o");                        // 왜
    EXPECT_EQ(render(hangul({0xC30D})), "+t k d");                       // 쌍 (tensed initial = Shift)
    EXPECT_EQ(render(hangul({0xAE4D})), "+r k r");                       // 깍
    EXPECT_EQ(render(hangul({0xBDC1})), "q n p f r");                    // 뷁 (compound vowel + compound final)
    EXPECT_EQ(render(hangul({0xAC00})), "r k");                          // 가: first syllable
    EXPECT_EQ(render(hangul({0xD7A3})), "g l g");                        // 힣: last syllable
}

TEST(PhoneKeys, hangul_jamo)
{
    EXPECT_EQ(render(hangul({0x314E})), "g");                            // ㅎ
    EXPECT_EQ(render(hangul({0x3152})), "+o");                           // ㅒ
    EXPECT_EQ(render(hangul({0x3156})), "+p");                           // ㅖ
    EXPECT_EQ(render(hangul({0x3143, 0x3149, 0x3138, 0x3132, 0x3146})), "+q +w +e +r +t");   // ㅃㅉㄸㄲㅆ
    EXPECT_EQ(render(hangul({0x3158, 0x3159, 0x315A, 0x315D, 0x315E, 0x315F, 0x3162})),
              "h k h o h l n j n p n l m l");                            // ㅘㅙㅚㅝㅞㅟㅢ
    EXPECT_EQ(render(hangul({0x3133, 0x3135, 0x3136, 0x313A, 0x313B, 0x313C, 0x313D, 0x313E, 0x313F, 0x3140, 0x3144})),
              "r t s w s g f r f a f q f t f x f v f g q t");           // compound finals
}

TEST(PhoneKeys, hangul_home_row_layout)
{
    // 두벌식 key positions for the plain jamo
    EXPECT_EQ(render(hangul({0x3142, 0x3148, 0x3137, 0x3131, 0x3145, 0x315B, 0x3155, 0x3151, 0x3150, 0x3154})), "q w e r t y u i o p");
    EXPECT_EQ(render(hangul({0x3141, 0x3134, 0x3147, 0x3139, 0x314E, 0x3157, 0x3153, 0x314F, 0x3163})), "a s d f g h j k l");
    EXPECT_EQ(render(hangul({0x314B, 0x314C, 0x314A, 0x314D, 0x3160, 0x315C, 0x3161})), "z x c v b n m");
}

TEST(PhoneKeys, hangul_syllable_equals_jamo_sequence)
{
    // 각 = ㄱ + ㅏ + ㄱ, 흙 = ㅎ + ㅡ + ㄺ
    EXPECT_EQ(phone::textToStrokes(hangul({0xAC01})).size(), phone::textToStrokes(hangul({0x3131, 0x314F, 0x3131})).size());
    EXPECT_EQ(render(hangul({0xD759})), render(hangul({0x314E, 0x3161, 0x313A})));
}

TEST(PhoneKeys, every_syllable_and_jamo_converts)
{
    for (ushort c = 0xAC00; c <= 0xD7A3; ++c) {
        int skipped = -1;
        const int n = phone::textToStrokes(QString(QChar(c)), &skipped).size();
        ASSERT_GE(n, 2) << c;
        ASSERT_LE(n, 5) << c;
        ASSERT_EQ(skipped, 0) << c;
    }
    for (ushort c = 0x3131; c <= 0x3163; ++c) {
        int skipped = -1;
        ASSERT_GE(phone::textToStrokes(QString(QChar(c)), &skipped).size(), 1) << c;
        ASSERT_EQ(skipped, 0) << c;
    }
}

TEST(PhoneKeys, mixed_text)
{
    int skipped = -1;
    const QString text = hangul({0xC548, 0xB155}) + " hi " + QChar(0xE9) + "!";   // 안녕 hi e-acute !
    EXPECT_EQ(render(text, &skipped), "d k s s u d Space h i Space +1");
    EXPECT_EQ(skipped, 1);
}

TEST(PhoneKeys, key_names_round_trip)
{
    for (int k = static_cast<int>(phone::Key::A); k <= static_cast<int>(phone::Key::HangulToggle); ++k) {
        const phone::Key key = static_cast<phone::Key>(k);
        const QString name = phone::keyName(key);
        ASSERT_FALSE(name.isEmpty()) << k;
        phone::Key back = phone::Key::None;
        ASSERT_TRUE(phone::keyFromName(name, &back)) << name.toStdString();
        ASSERT_EQ(back, key) << name.toStdString();
    }
}

TEST(PhoneKeys, key_names_specific_and_invalid)
{
    EXPECT_EQ(phone::keyName(phone::Key::None), QString());
    EXPECT_EQ(phone::keyName(phone::Key::F1), "F1");
    EXPECT_EQ(phone::keyName(phone::Key::F12), "F12");
    EXPECT_EQ(phone::keyName(phone::Key::Digit0), "0");
    EXPECT_EQ(phone::keyName(phone::Key::Backslash), "\\");
    EXPECT_EQ(phone::keyName(phone::Key::HangulToggle), "HangulToggle");

    phone::Key k = phone::Key::Enter;
    for (const char* bad : {"", "A", "enter", "F0", "F13", "f1", "Ctrl ", "None", "ab", "\xEC\x95\x88"})
        EXPECT_FALSE(phone::keyFromName(QString::fromUtf8(bad), &k)) << bad;
    EXPECT_EQ(k, phone::Key::Enter);   // untouched on failure
    EXPECT_TRUE(phone::keyFromName("a", nullptr));
}

TEST(PhoneKeys, modifiers)
{
    EXPECT_TRUE(phone::isModifier(phone::Key::Ctrl));
    EXPECT_TRUE(phone::isModifier(phone::Key::Alt));
    EXPECT_TRUE(phone::isModifier(phone::Key::Shift));
    EXPECT_TRUE(phone::isModifier(phone::Key::Meta));
    EXPECT_FALSE(phone::isModifier(phone::Key::A));
    EXPECT_FALSE(phone::isModifier(phone::Key::None));
    EXPECT_FALSE(phone::isModifier(phone::Key::HangulToggle));
}

namespace {

using Mod = BaseConfig::Modifier;
using phone::Key;

// screen mapping from {from, to} pairs; anything not listed arrives unchanged
std::function<Mod(Mod)> mapping(std::initializer_list<std::pair<Mod, Mod>> pairs)
{
    const std::vector<std::pair<Mod, Mod>> table(pairs);
    return [table](Mod m) {
        for (const auto& p : table)
            if (p.first == m)
                return p.second;
        return m;
    };
}

} // namespace

TEST(PhoneKeys, localModifierKeys_identity)
{
    const QMap<Key, Key> keys = phone::localModifierKeys(mapping({}));
    EXPECT_EQ(keys.size(), 4);
    for (Key k : {Key::Shift, Key::Ctrl, Key::Alt, Key::Meta})
        EXPECT_EQ(keys.value(k), k);
}

TEST(PhoneKeys, localModifierKeys_swapped)
{
    // Mac server, Windows screen with "super = alt, alt = super" (Command acts as Alt there)
    const QMap<Key, Key> keys = phone::localModifierKeys(mapping({{Mod::Super, Mod::Alt}, {Mod::Alt, Mod::Super}}));
    EXPECT_EQ(keys.value(Key::Alt), Key::Meta);    // Windows Alt <- press Command
    EXPECT_EQ(keys.value(Key::Meta), Key::Alt);    // Windows key <- press Option
    EXPECT_EQ(keys.value(Key::Ctrl), Key::Ctrl);
    EXPECT_EQ(keys.value(Key::Shift), Key::Shift);
}

TEST(PhoneKeys, localModifierKeys_notInjective)
{
    // "super = ctrl" only: Ctrl is reached by both, nothing reaches the Windows key any more
    const QMap<Key, Key> keys = phone::localModifierKeys(mapping({{Mod::Super, Mod::Ctrl}}));
    EXPECT_EQ(keys.value(Key::Ctrl), Key::Ctrl);   // the key itself wins a tie
    EXPECT_FALSE(keys.contains(Key::Meta));
    // "alt = none": Alt is unreachable, and None never counts as a target
    const QMap<Key, Key> none = phone::localModifierKeys(mapping({{Mod::Alt, Mod::None}}));
    EXPECT_FALSE(none.contains(Key::Alt));
    EXPECT_EQ(none.value(Key::Meta), Key::Meta);
}

TEST(PhoneKeys, macGestureHotKey)
{
    phone::MacHotKey hk;
    // never changed: the macOS default, Control+fn+Up
    EXPECT_TRUE(phone::macGestureHotKey(QStringLiteral("up"), {}, &hk));
    EXPECT_EQ(hk.keyCode, 126);
    EXPECT_EQ(hk.flags, 0x840000u);
    // rebound by the user: "show desktop" on Option+D (this repo owner's real setting)
    const QVariantMap desktop{{QStringLiteral("enabled"), true},
                              {QStringLiteral("value"), QVariantMap{{QStringLiteral("parameters"),
                                                                     QVariantList{100, 2, 524288}}}}};
    EXPECT_TRUE(phone::macGestureHotKey(QStringLiteral("pinchout"), {{QStringLiteral("36"), desktop}}, &hk));
    EXPECT_EQ(hk.keyCode, 2);
    EXPECT_EQ(hk.flags, 0x80000u);
    // listed and on but without parameters (as on the owner's Mac): the default key
    EXPECT_TRUE(phone::macGestureHotKey(QStringLiteral("left"),
                                        {{QStringLiteral("81"), QVariantMap{{QStringLiteral("enabled"), true}}}}, &hk));
    EXPECT_EQ(hk.keyCode, 124);
    EXPECT_EQ(hk.flags, 0x840000u);
    // turned off, unassigned (65535) and gestures without a shortcut
    EXPECT_FALSE(phone::macGestureHotKey(QStringLiteral("left"),
                                         {{QStringLiteral("81"), QVariantMap{{QStringLiteral("enabled"), false}}}}, &hk));
    const QVariantMap unbound{{QStringLiteral("enabled"), true},
                              {QStringLiteral("value"), QVariantMap{{QStringLiteral("parameters"),
                                                                     QVariantList{65535, 65535, 0}}}}};
    EXPECT_FALSE(phone::macGestureHotKey(QStringLiteral("down"), {{QStringLiteral("33"), unbound}}, &hk));
    EXPECT_FALSE(phone::macGestureHotKey(QStringLiteral("pinchin"), {}, &hk));
    EXPECT_FALSE(phone::macGestureHotKey(QStringLiteral("rm -rf"), {}, &hk));
}
