#pragma once
#include <cmath>

// Circular ("rotary") scrolling: the thumb circles the middle of a pad the way it
// would turn a wheel, and the page scrolls by how far round it has gone — not by
// how far across the pad it has travelled. This is what Steam's own scroll-wheel
// pad mode does, and why a long scroll is one lazy circle rather than a string of
// swipes with a lift between each.
//
// Kept apart from TrackpadInput because it is arithmetic on positions and nothing
// else: no SendInput, no Windows, so the maths can be checked without a
// controller or a desktop. See RotaryScrollProbe.

// The pad's coordinate space is square, both axes saturating at +/-32767, so this
// is raw report units.
//
// Inside this radius the angle is not trustworthy: the centroid wobbles by a few
// hundred units frame to frame, which near the middle spans whole radians. A
// thumb that wanders into the middle drops its reference instead and picks a new
// one when it comes back out, so passing through never scrolls.
inline constexpr int kRotaryDeadRadius = 5000;

// A thumb cannot move a quarter of a turn between two reports (4ms apart at the
// rates both transports use). A step this large is a lift and re-placement the
// touch bit did not report, or a sensor jump, and applying it would throw the page
// a long way for nothing.
inline constexpr float kRotaryMaxStepRadians = 1.2f;

// Wheel notches for one full circle at 100% speed. Twelve is a comfortable
// reading-pace scroll: a quarter turn is three notches, a few lines each.
inline constexpr float kRotaryNotchesPerTurn = 12.0f;

inline constexpr float kRotaryPi = 3.14159265358979323846f;

// WHEEL_DELTA is 120. Spelled out here so this header stays free of Windows.h.
inline constexpr float kRotaryWheelUnitsPerRadian =
    kRotaryNotchesPerTurn * 120.0f / (2.0f * kRotaryPi);

class RotaryTracker {
public:
    // Forget where the finger was. Call on a lift, a press, or any change that
    // makes the previous angle mean something different.
    void Reset() { m_have = false; }

    // Advances by one frame in which the finger is on the pad at (x, y), with Y
    // growing upward as the report has it. Returns how far it went round since
    // the previous call, in radians, positive counter-clockwise; zero whenever
    // there is no usable reference — the first frame, a frame inside the dead
    // radius, or a step too large to be a finger.
    float Step(int x, int y) {
        const double r = std::hypot(static_cast<double>(x), static_cast<double>(y));
        if (r < static_cast<double>(kRotaryDeadRadius)) {
            m_have = false;
            return 0.0f;
        }

        const float angle = static_cast<float>(
            std::atan2(static_cast<double>(y), static_cast<double>(x)));
        if (!m_have) {
            m_prev = angle;
            m_have = true;
            return 0.0f;
        }

        float d = angle - m_prev;
        // The angle jumps by a full turn where atan2 wraps from +pi to -pi, and
        // that is not movement: take the short way round.
        if (d >  kRotaryPi) d -= 2.0f * kRotaryPi;
        if (d < -kRotaryPi) d += 2.0f * kRotaryPi;
        m_prev = angle;
        if (std::fabs(d) > kRotaryMaxStepRadians) return 0.0f;
        return d;
    }

    bool HasReference() const { return m_have; }

private:
    bool  m_have = false;
    float m_prev = 0.0f;
};
