/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/PhoneInputMarker.h"

#include <gtest/gtest.h>

using inputleap::takeWhole;

TEST(PhoneInputMarkerTests, takeWhole_smallDeltasAddUp)
{
    double acc = 0;
    EXPECT_EQ(takeWhole(&acc, 0.4), 0);
    EXPECT_EQ(takeWhole(&acc, 0.4), 0);
    EXPECT_EQ(takeWhole(&acc, 0.4), 1);
    EXPECT_NEAR(acc, 0.2, 1e-9);
    EXPECT_EQ(takeWhole(&acc, 2.9), 3);
    EXPECT_NEAR(acc, 0.1, 1e-9);
}

TEST(PhoneInputMarkerTests, takeWhole_negativeAndReversal)
{
    double acc = 0;
    EXPECT_EQ(takeWhole(&acc, -0.7), 0);
    EXPECT_EQ(takeWhole(&acc, -0.7), -1);
    EXPECT_NEAR(acc, -0.4, 1e-9);
    // turning back drops the -0.4 instead of eating the first +0.6 with it
    EXPECT_EQ(takeWhole(&acc, 0.6), 0);
    EXPECT_NEAR(acc, 0.6, 1e-9);
    EXPECT_EQ(takeWhole(&acc, 0.4), 1);
}

TEST(PhoneInputMarkerTests, takeWhole_wholeNotchesStayExact)
{
    // OSXScreen::fakeMouseWheel: a normal 120-unit notch is 3 lines with nothing left over
    double acc = 0;
    EXPECT_EQ(takeWhole(&acc, 3.0 * 120 / 120.0), 3);
    EXPECT_EQ(takeWhole(&acc, 3.0 * -240 / 120.0), -6);
    EXPECT_EQ(acc, 0.0);
    // a 30-unit smooth-scroll step is 0.75 line: nothing now, one line on the second step
    EXPECT_EQ(takeWhole(&acc, 3.0 * 30 / 120.0), 0);
    EXPECT_EQ(takeWhole(&acc, 3.0 * 30 / 120.0), 1);
}
