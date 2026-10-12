#pragma once
#include <chrono>
#include <cstdint>
#include <cstddef>
#include "RotaryScroll.h"
#include "TrackpadConfig.h"

// What ONE physical trackpad's finger position means. One instance per pad, so
// the two pads can be configured independently — this used to be a single
// instance switched between pads by a bool, which structurally could not
// represent "both pads doing something".
//
// Two jobs, both of them "turn a position into something". The pointer and
// scroll modes drive the desktop directly from here. Directional pad mode
// resolves nothing itself: it works out which direction the thumb is pressing
// and hands that out through Directions(), because a direction has to reach
// both the virtual gamepad and SendInput and neither of those belongs here.
//
// No binding is dispatched from this class. A pad's click, touch and
// directions are all BackButtonBindings, dispatched by ControllerManager
// alongside the back paddles, which is what lets them be the same kind of
// thing as a paddle whatever this pad's mode is.
class TrackpadInput {
public:
    // Which physical pad to read. Set once when the slot is created.
    void SetPad(bool isLeftPad) { m_isLeftPad = isLeftPad; }
    void SetMode(TrackpadMode mode);
    void SetScrollDirection(ScrollDirection dir) { m_scrollDir = dir; }
    void SetScrollStyle(ScrollStyle style);
    // Percent of the calibrated scroll scale — see kScrollSpeedDefault.
    // Clamped here rather than trusted, since it arrives from the registry.
    void SetScrollSpeed(uint32_t percent) { m_scrollSpeed = ClampScrollSpeed(percent); }
    // Percent strength of the speed-shaped scroll acceleration — see
    // kScrollAccelDefault. Clamped here rather than trusted, since it arrives
    // from the registry.
    void SetScrollAccel(uint32_t percent) { m_scrollAccel = ClampScrollAccel(percent); }
    // Percent of the calibrated trackball-glide feel — see
    // kGlideStrengthDefault. Clamped here rather than trusted, since it
    // arrives from the registry.
    void SetGlideStrength(uint32_t percent) { m_glideStrength = ClampGlideStrength(percent); }
    // Percent of the calibrated pointer pace — see kPointerSpeedDefault.
    // Clamped here rather than trusted, since it arrives from the registry.
    void SetPointerSpeed(uint32_t percent) { m_pointerSpeed = ClampPointerSpeed(percent); }
    // Percent strength of the speed-shaped pointer acceleration — see
    // kPointerAccelDefault. Clamped here rather than trusted, since it arrives
    // from the registry.
    void SetPointerAccel(uint32_t percent) { m_pointerAccel = ClampPointerAccel(percent); }
    // Raw pad units — see kPointerDeadzoneDefault. Clamped here rather than
    // trusted, since it arrives from the registry.
    void SetPointerDeadzone(uint32_t units) { m_pointerDeadzone = ClampPointerDeadzone(units); }
    void SetDiagonals(DiagonalMode d);

    // `pressed` is whether the pad is being pressed, worked out from contact
    // area by the caller — the firmware's click bit reports fewer than half of
    // them. See kPadPressArea.
    void Update(const uint8_t* buf, size_t n, bool pressed);
    void Reset();

    // Which directions this pad is pressing right now, as PadDir bits. Always
    // empty unless the pad is a directional pad with its click held.
    uint8_t Directions() const { return m_dirs; }

    // Where the click being held landed: inside the centre circle, where the
    // press is the pad's click binding, or out in the ring, where it is a
    // direction instead. True for any pad that is not a directional pad, so a
    // caller can gate the click binding on this unconditionally.
    //
    // Meaningless while no click is held, which costs nothing — every caller
    // is already testing the click bit.
    bool ClickInCentre() const { return m_clickInCentre; }

private:
    // `accel` multiplies PointerSensitivity on top of the usual sub-pixel
    // remainder carry. Defaults to 1x (no change) for StepFling's coasting
    // calls, which have their own velocity-based physics already — see
    // SpeedAccelFactor for where the live-movement call site gets a computed
    // one instead.
    void UpdatePointer(int dx, int dy, float accel = 1.0f);
    // Trackball-style glide — see the block of constants and methods in the
    // .cpp for how the three fit together: sample while touching, hand off on
    // lift, step while coasting.
    void SampleFlingVelocity(int dxRaw, int dyRaw);
    void StartFling();
    void StepFling();
    float ScrollScale() const;
    float RotaryScale() const;
    // Seconds since the last call, for whichever scroll style is active (the
    // two never run at once, so one clock serves both). Zero on the first
    // call after a reset — a speed needs two points, not one.
    float ScrollDt();
    // Shapes an instantaneous speed into a multiplier on ScrollScale/
    // RotaryScale/PointerSensitivity: below `neutral` reads finer than the
    // calibrated feel, for precision; at `neutral` it is unchanged; above it,
    // faster, for reach on a fast flick. `neutral` is in whatever unit `speed`
    // is — raw pad units/second for a swipe or the pointer, radians/second for
    // a turn — so the same curve serves all three from their own reference
    // point. `strengthPercent` blends between flat (0%, always 1x regardless
    // of speed) and the curve at full strength (100%) and beyond — see
    // kScrollAccelDefault / kPointerAccelDefault.
    static float SpeedAccelFactor(float speed, float neutral, uint32_t strengthPercent);
    void UpdateScroll(int dx, int dy);
    // `radians` is how far round the pad the finger went this frame, positive
    // counter-clockwise — see RotaryTracker.
    void UpdateRotaryScroll(float radians);
    void NoteMovementSent(long px, bool haveCursor, long cursorX, long cursorY);
    // Resolves the eight- or four-way sector a press at this angle belongs to,
    // holding the current sector until the thumb clears its boundary by the
    // hysteresis margin. Reads m_sector; does not write it.
    int  ResolveSector(double angleDeg) const;
    void UpdateDirections(bool clicked, int16_t x, int16_t y);
    // Diagnostic only — reports single frames that moved further than a finger
    // can, and how long since the previous one, which is what separates a
    // sensor jump from reports going missing. Changes nothing about what is
    // sent. See the definition.
    void NoteJump(const uint8_t* buf, size_t n, int dx, int dy);

    bool            m_isLeftPad = false;
    TrackpadMode    m_mode      = TrackpadMode::None;
    ScrollDirection m_scrollDir = ScrollDirection::Natural;
    ScrollStyle     m_scrollStyle = ScrollStyle::Swipe;
    RotaryTracker   m_rotary;
    uint32_t        m_scrollSpeed = kScrollSpeedDefault;
    uint32_t        m_scrollAccel = kScrollAccelDefault;
    uint32_t        m_glideStrength = kGlideStrengthDefault;
    uint32_t        m_pointerSpeed = kPointerSpeedDefault;
    uint32_t        m_pointerAccel = kPointerAccelDefault;
    uint32_t        m_pointerDeadzone = kPointerDeadzoneDefault;
    DiagonalMode    m_diagonals = DiagonalMode::EightWay;

    bool     m_touching  = false;
    int16_t  m_prevX     = 0;
    int16_t  m_prevY     = 0;
    float    m_remX      = 0.0f;  // sub-pixel movement carry
    float    m_remY      = 0.0f;
    float    m_scrollRemX = 0.0f;  // sub-detent scroll carry
    float    m_scrollRemY = 0.0f;

    // Movement Windows accepted, checked against the cursor actually moving.
    // A refused SendInput reports itself; motion that is accepted and then
    // discarded — by a cursor clip a game left behind, or a low-level hook
    // filtering injected input — looks identical to the user and otherwise
    // leaves no trace at all. Records where the cursor was and how much travel
    // has been sent since; when the travel adds up and the position has not
    // changed, that is worth a log line.
    long     m_refCursorX  = 0;
    long     m_refCursorY  = 0;
    bool     m_haveRef     = false;
    long     m_travelSent  = 0;

    // Enough sent movement that a still cursor cannot be a coincidence.
    static constexpr long  STUCK_TRAVEL_PX = 120;

    // The pace at 100% pointer speed (kPointerSpeedDefault), in pixels per raw
    // pad unit. 1.5x slower than the single fixed value every build before
    // kPointerSpeedDefault existed used (0.01125) — that pace turned a full
    // pad swipe into more cursor travel than was wanted, and 100% is defined
    // as the toned-down pace rather than the old one so a profile that has
    // never touched this setting gets the fix automatically. See
    // PointerSensitivity for how m_pointerSpeed scales this.
    static constexpr float POINTER_SENSITIVITY = 0.01125f / 1.5f;
    // Chosen so a full swipe across the pad is a few notches rather than a
    // page-length fling: each pad axis saturates at +/-32767, and one wheel
    // detent is WHEEL_DELTA (120).
    static constexpr float SCROLL_SENSITIVITY = 0.02f;
    // Pixels per raw pad unit at the current pointer speed setting — the
    // pointer-mode equivalent of ScrollScale, for the same reason: a taste
    // setting applied on top of a fixed, calibrated pace rather than baked
    // into it.
    float PointerSensitivity() const {
        return POINTER_SENSITIVITY * (static_cast<float>(m_pointerSpeed) / 100.0f);
    }

    // ---- Trackball-style glide (MousePointer only) ----

    // Smoothed release velocity, in raw pad units/second — the same units as
    // the x/y deltas UpdatePointer already scales by SENSITIVITY. Carried
    // across touched frames so a lift reads the recent swipe speed rather
    // than one noisy frame right at the edge.
    float m_velX = 0.0f;
    float m_velY = 0.0f;
    // False until the first touched frame after a press has a previous frame
    // to diff against — a velocity needs two points, not one.
    bool  m_haveFlingVel = false;
    // Coasting with no finger on the pad right now.
    bool  m_flinging = false;
    // Shared clock for both jobs above: the dt between touched frames while
    // sampling, and the dt between glide steps while coasting. The two never
    // overlap in a given Update() call, so one timestamp covers both.
    std::chrono::steady_clock::time_point m_lastInertiaAt{};
    bool m_haveInertiaClock = false;

    // How much of the previous velocity estimate survives each new sample.
    // Low enough that the swipe's last couple of frames dominate the number
    // handed to the glide, high enough that one noisy frame right at the lift
    // is not mistaken for the whole flick's speed.
    static constexpr float FLING_VELOCITY_SMOOTHING = 0.75f;
    // Below this speed (raw pad units/second) a lift is a stop, not a flick.
    // Resting the thumb down and lifting it gently must never launch a
    // glide — only a swipe fast enough to cross most of the pad in well under
    // a second does. Independent of m_glideStrength: that setting scales how
    // far a recognised flick then coasts, not what counts as one.
    //
    // 6000 here originally was a units-per-FRAME number mistaken for a
    // units-per-SECOND one — about the scale NoteJump's own per-frame anomaly
    // threshold uses, not a per-second speed. Converted through SENSITIVITY it
    // is only 67.5 px/second, slower than an ordinary deliberate drag, so
    // nearly every lift read as a flick — which is exactly the bug report
    // this fixes: the cursor kept gliding after an unhurried release. 100000
    // is 1125 px/second: crossing the whole pad (65534 units) at exactly this
    // speed takes ~0.66s, and a real flick is faster than its own minimum, so
    // this is a floor under "well under a second", not the flick speed
    // itself. Still a first cut without a controller in hand to flick for
    // real — tune it if a deliberate fast swipe still does not glide, or a
    // merely brisk drag still does.
    static constexpr float FLING_MIN_SPEED = 100000.0f;
    // Fraction of speed that survives one full second of coasting, at 100%
    // glide strength. m_glideStrength scales the velocity handed to the glide
    // in StartFling rather than this constant, so the decay shape stays the
    // same at every strength and only the distance changes.
    static constexpr float FLING_DECAY_PER_SEC = 0.05f;
    // Below this speed the glide is imperceptible; stopped exactly here
    // rather than left to approach zero forever.
    static constexpr float FLING_STOP_SPEED = 400.0f;

    // ---- Speed-shaped acceleration + anti-jitter dead zones ----
    //
    // SpeedAccelFactor applies to ScrollWheel (both styles) and MousePointer
    // alike: slower than a comfortable pace reads finer than the calibrated
    // 100% feel (precision), faster reads coarser (reach on a flick) — the
    // way trackpad scrolling on macOS accelerates with a fast swipe and
    // settles to fine control for a slow one, rather than one scale for every
    // speed, and the same idea applied to the cursor. m_scrollAccel and
    // m_pointerAccel each control how strongly their own pad's movement bends
    // to this curve; see kScrollAccelDefault / kPointerAccelDefault.
    //
    // The dead zones below are unrelated to the curve and stay fixed rather
    // than becoming settings of their own: they absorb the jitter a
    // 100%-faithful per-frame scale would otherwise turn into a stutter — the
    // same contact-centroid settling documented at kPointerDeadzoneDefault,
    // here showing up as noise on the scroll axis instead of a cursor jump.
    // Accumulated rather than discarded, the same way as the pointer's — see
    // UpdateScroll and UpdateRotaryScroll. Tell me if the feel needs tuning
    // and I will either adjust these or turn them into sliders too.

    // Curve shape: speed is raised to this power after dividing by the style's
    // neutral speed, so values below 1x shrink (more than linearly) and values
    // above it grow the same way — one exponent gives both the precision at
    // low speed and the acceleration at high speed the request asked for.
    // Shared by all three domains; m_scrollAccel/m_pointerAccel are what let
    // each bend to it independently.
    static constexpr float ACCEL_GAMMA = 1.6f;
    // Floor and ceiling on the curve itself, before the strength blend. The
    // floor keeps a truly glacial movement from reading as "basically
    // nothing"; the ceiling caps how far one flick can throw the page or the
    // cursor.
    static constexpr float ACCEL_MIN = 0.35f;
    static constexpr float ACCEL_MAX = 3.0f;
    // The speed at which a swipe-style pad's acceleration factor is 1x — a
    // comfortable, unhurried scrolling swipe, in raw pad units/second. Below
    // it the scale shrinks for precision; above it, a faster swipe, the scale
    // grows. A first cut without a controller in hand to swipe for real.
    static constexpr float SWIPE_NEUTRAL_SPEED = 30000.0f;
    // The rotary equivalent, in radians/second: about half a turn per second,
    // a comfortable circling pace for reading-speed scrolling.
    static constexpr float ROTARY_NEUTRAL_SPEED = 3.5f;
    // The pointer equivalent, in raw pad units/second — a comfortable,
    // unhurried cursor drag. Pointer movement is more often slow and
    // deliberate (aiming) than scroll swiping is, which is why this sits a
    // little below SWIPE_NEUTRAL_SPEED rather than sharing it: the same raw
    // speed should read as "a bit brisk" for the cursor sooner than it does
    // for a scroll swipe.
    static constexpr float POINTER_NEUTRAL_SPEED = 25000.0f;

    // Swipe-style dead zone, in raw pad units — the position-delta equivalent
    // of kPointerDeadzoneDefault, filtering the same kind of settling on the
    // scroll axis instead of the cursor. Applied at the Update() call site by
    // leaving m_prevX/Y uncommitted below this, exactly like the pointer's.
    static constexpr double SCROLL_DEADZONE_UNITS = 300.0;
    // Rotary-style dead zone, in radians — small angular jitter accumulates in
    // m_rotaryAccum instead of ticking the wheel on every sub-threshold wobble.
    // ~1.4 degrees: big enough to absorb sensor noise near the dead radius
    // boundary, small enough not to noticeably lag a deliberate turn.
    static constexpr float ROTARY_DEADZONE_RADIANS = 0.025f;

    // ScrollDt's clock — shared by swipe and rotary since only one runs at a
    // time per pad.
    std::chrono::steady_clock::time_point m_lastScrollAt{};
    bool  m_haveScrollClock = false;
    // Rotary's dead-zone accumulator, in radians. Reset alongside m_rotary
    // itself: a reference drop means whatever had been building up no longer
    // describes a continuous turn.
    float m_rotaryAccum = 0.0f;

    // ---- Directional pad ----

    // The directions currently pressed, and the sector they came from. The
    // sector is kept separately because hysteresis needs to know which one is
    // being held, which the bitmask cannot say: up-left and left-up are the
    // same two bits.
    uint8_t m_dirs   = DirNone;
    int     m_sector = -1;      // -1 = none held

    // A click is being held, and where it landed. The zone is decided once, on
    // the press, and never revisited: sliding from the ring into the centre
    // mid-press would otherwise release a direction and fire the click
    // binding, which is one physical press meaning two different things — the
    // exact collision the zone split exists to prevent.
    bool m_clickHeld     = false;
    bool m_clickInCentre = true;

    // How far past a sector boundary the thumb has to travel before the
    // direction changes. Without it a thumb parked on a boundary alternates
    // between two directions at the report rate, and since directions are
    // edge-dispatched that is a stream of key down/up pairs rather than a
    // cosmetic flicker.
    static constexpr double SECTOR_HYSTERESIS_DEG = 8.0;

    // ---- Movement jump reporting (diagnostic) ----

    // Far enough in one frame that no finger did it. The pad's axes saturate
    // at +/-32767, so this is roughly a fifth of the way across in a single
    // report — well past a fast flick at any report rate either transport
    // uses, and nowhere near the pad-width gap a missed lift produces.
    static constexpr double JUMP_REPORT_UNITS = 6000.0;
    // A pad that jumps once jumps often. A handful of examples says everything
    // a flood would, so the rest are dropped.
    static constexpr double JUMP_LOG_GAP_S = 3.0;

    std::chrono::steady_clock::time_point m_lastFrameAt{};
    std::chrono::steady_clock::time_point m_lastJumpLogAt{};
    bool m_haveFrameTime  = false;
    bool m_haveLoggedJump = false;
};
