#pragma once
#include <cstdint>
#include <string>
#include "BackButtonConfig.h"
#include "ButtonConfig.h"

// Button combos ("chords"): hold two inputs down together and a bound action
// fires instead of — not alongside — whatever either input would otherwise do
// on its own. The model is Steam's own default of clicking both sticks
// together to open the on-screen keyboard: two ordinary inputs, each useful
// alone, that mean something different held at once.
//
// Scope is deliberately two members, not an arbitrary set: both of the
// combos this shipped with are pairs, a pair is what the request asked for,
// and a third member multiplies the "what if two combos partially overlap"
// questions below for a case nobody has asked for yet.

// One physical input a chord can require held. Values are stable — persisted
// as part of a packed DWORD — and None is 0 so an unconfigured slot reads as
// "no chord" rather than silently claiming button A.
//
// BtnA..BtnSteam are laid out in exactly PadButton's own order (see
// ButtonConfig.h) starting right after None. That is what lets
// ChordMemberToPadButton recover a PadButton by subtracting BtnA instead of a
// second table mapping one onto the other that could drift out of step with
// it — keep the two orderings in sync if either ever changes.
enum class ChordMember : uint8_t {
    None = 0,
    BtnA, BtnB, BtnX, BtnY, BtnLB, BtnRB,
    BtnUp, BtnDown, BtnLeft, BtnRight,
    BtnMenu, BtnView, BtnL3, BtnR3, BtnSteam,
    PaddleL4, PaddleL5, PaddleR4, PaddleR5,
    LeftPadClick, RightPadClick,
    COUNT
};

// The PadButton a button-kind member names, for every member from BtnA to
// BtnSteam. False for the four paddles, the two pad clicks, and None — none
// of which has a PadButton to recover.
inline bool ChordMemberToPadButton(ChordMember m, PadButton& out) {
    if (m < ChordMember::BtnA || m > ChordMember::BtnSteam) return false;
    out = static_cast<PadButton>(static_cast<int>(m) - static_cast<int>(ChordMember::BtnA));
    return true;
}

// Wire ids match the ones PadButtonWireId and the paddle/pad-click rows
// already use elsewhere on the page, so the same catalog of names serves both
// without a translation table between them.
inline const char* ChordMemberId(ChordMember m) {
    switch (m) {
    case ChordMember::BtnA:          return "BTN_A";
    case ChordMember::BtnB:          return "BTN_B";
    case ChordMember::BtnX:          return "BTN_X";
    case ChordMember::BtnY:          return "BTN_Y";
    case ChordMember::BtnLB:         return "BTN_LB";
    case ChordMember::BtnRB:         return "BTN_RB";
    case ChordMember::BtnUp:         return "BTN_Up";
    case ChordMember::BtnDown:       return "BTN_Down";
    case ChordMember::BtnLeft:       return "BTN_Left";
    case ChordMember::BtnRight:      return "BTN_Right";
    case ChordMember::BtnMenu:       return "BTN_Menu";
    case ChordMember::BtnView:       return "BTN_View";
    case ChordMember::BtnL3:         return "BTN_L3";
    case ChordMember::BtnR3:         return "BTN_R3";
    case ChordMember::BtnSteam:      return "BTN_Steam";
    case ChordMember::PaddleL4:      return "L4";
    case ChordMember::PaddleL5:      return "L5";
    case ChordMember::PaddleR4:      return "R4";
    case ChordMember::PaddleR5:      return "R5";
    case ChordMember::LeftPadClick:  return "LPAD";
    case ChordMember::RightPadClick: return "RPAD";
    default:                         return "none";
    }
}

inline ChordMember ChordMemberFromId(const std::string& id) {
    for (uint32_t v = 1; v < static_cast<uint32_t>(ChordMember::COUNT); ++v) {
        const auto m = static_cast<ChordMember>(v);
        if (id == ChordMemberId(m)) return m;
    }
    return ChordMember::None;
}

// Anything a future build might write that this one does not understand
// degrades to None rather than being applied — the same rule
// TrackpadModeFromDword and friends use.
inline ChordMember ChordMemberFromDword(uint32_t v) {
    return v < static_cast<uint32_t>(ChordMember::COUNT)
         ? static_cast<ChordMember>(v)
         : ChordMember::None;
}

// How many combo slots a profile carries. Two ship with defaults (see
// ControllerProfile in TrackpadConfig.h); the rest start empty, for whatever
// combos someone wants of their own. Picked as "a handful of spares" rather
// than a dynamic list — every slot is already fully configurable, so a fixed
// row of them is simpler than add/remove bookkeeping for the common case of
// wanting one or two more than the defaults.
inline constexpr size_t kChordSlotCount = 6;

// One combo slot: two members and the binding that fires while both are held.
// The action is a BackButtonBinding rather than a type of its own for the same
// reason a trackpad click or a trigger's full press is: it is the same
// question ("what should this input do"), answered by the same catalog and
// dispatched down the same path as a paddle.
struct ChordBinding {
    ChordMember       a      = ChordMember::None;
    ChordMember       b      = ChordMember::None;
    BackButtonBinding action;

    // Both members chosen, distinct from each other, and an action actually
    // bound. Anything less does nothing — a slot with two members picked but
    // no action set yet must not start suppressing them, the same way a
    // trigger's full press does nothing until it carries a binding.
    bool IsConfigured() const {
        return a != ChordMember::None && b != ChordMember::None && a != b
            && !action.IsAction(BackButtonAction::None);
    }

    bool operator==(const ChordBinding& o) const {
        return a == o.a && b == o.b && action == o.action;
    }
    bool operator!=(const ChordBinding& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Persistence
//
// Same shape as ReadTriggerSettings/WriteTriggerSettings in TriggerConfig.h,
// read from two places that cannot share a helper — the default profile in
// TrayApp and every game profile in GameProfiles — for the same reason: so a
// combo's three values cannot drift apart between the two.
//
// `def` supplies what a missing value reads as, which differs per slot (the
// first two slots default to the combos this feature shipped with; the rest
// default to an unconfigured ChordBinding{}) — unlike a trigger's fields,
// there is no single sensible default every slot could share.
// ---------------------------------------------------------------------------

template <class ReadDw>
inline ChordBinding ReadChordBinding(const wchar_t* prefix, const ChordBinding& def,
                                     ReadDw&& readDw) {
    auto name = [&](const wchar_t* suffix) { return std::wstring(prefix) + suffix; };
    ChordBinding c;
    c.a = ChordMemberFromDword(readDw(name(L"A").c_str(), static_cast<uint32_t>(def.a)));
    c.b = ChordMemberFromDword(readDw(name(L"B").c_str(), static_cast<uint32_t>(def.b)));
    c.action = BackButtonBinding::Unpack(readDw(name(L"Action").c_str(), def.action.Pack()));
    return c;
}

template <class WriteDw>
inline void WriteChordBinding(const wchar_t* prefix, const ChordBinding& c, WriteDw&& writeDw) {
    auto name = [&](const wchar_t* suffix) { return std::wstring(prefix) + suffix; };
    writeDw(name(L"A").c_str(),      static_cast<uint32_t>(c.a));
    writeDw(name(L"B").c_str(),      static_cast<uint32_t>(c.b));
    writeDw(name(L"Action").c_str(), c.action.Pack());
}
