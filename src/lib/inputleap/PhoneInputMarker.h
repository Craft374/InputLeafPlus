/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeafPlus Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <cmath>

namespace inputleap {

// Tags input injected by the phone trackpad (GUI) so the server can tell it from hardware:
// macOS: kCGEventSourceUserData of injected mouse-move and scroll events, Windows: dwExtraInfo.
constexpr long long kPhoneInjectMarker = 0x504E4F53;

// Phone scrolling is in pixels; where it has to become a wheel (Windows, or a screen the Mac
// server forwards to) this many pixels make one 120-unit notch.
constexpr double kPhonePxPerNotch = 50.0;

// Adds delta to *acc and returns the whole units to emit now. The fraction stays in *acc, so a
// stream of small deltas still adds up instead of each one rounding to 0. A reversal drops the
// old leftover so the first step back is not swallowed by it.
inline int takeWhole(double* acc, double delta)
{
    if ((*acc > 0 && delta < 0) || (*acc < 0 && delta > 0)) {
        *acc = 0;
    }
    *acc += delta;
    const double whole = std::trunc(*acc);
    *acc -= whole;
    return static_cast<int>(whole);
}

} // namespace inputleap
