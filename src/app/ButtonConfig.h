#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "BackButtonConfig.h"

// The controller's ordinary buttons — everything that is not a paddle, a pad or a
// trigger — and what each one does.
//
// Until these were configurable a button could only ever be itself: A was the
// virtual pad's A and nothing else. Each one now carries a BackButtonBinding, the
// same catalog a paddle or a pad click draws on, so A can be Enter, Menu can be
// Escape and a stick click can be a mouse button.
//
// The default for every button is the gamepad action it has always been. That
// is what makes a missing value, a profile written before this existed and a
// freshly reset window all read as "behaves exactly as before" with no migration
// step, and it is why an unchanged button costs nothing: see Effective.
//
// Values are stable — they index a table and name registry values.
enum class PadButton : uint8_t {
    A = 0, B, X, Y,
    LB, RB,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    Menu, View,
    L3, R3,
    Steam,
    COUNT
};

inline constexpr size_t kPadButtonCount = static_cast<size_t>(PadButton::COUNT);

// The gamepad action a button is when nobody has remapped it.
inline BackButtonAction PadButtonIdentity(PadButton b) {
    switch (b) {
    case PadButton::A:          return BackButtonAction::A;
    case PadButton::B:          return BackButtonAction::B;
    case PadButton::X:          return BackButtonAction::X;
    case PadButton::Y:          return BackButtonAction::Y;
    case PadButton::LB:         return BackButtonAction::LB;
    case PadButton::RB:         return BackButtonAction::RB;
    case PadButton::DPadUp:     return BackButtonAction::DPadUp;
    case PadButton::DPadDown:   return BackButtonAction::DPadDown;
    case PadButton::DPadLeft:   return BackButtonAction::DPadLeft;
    case PadButton::DPadRight:  return BackButtonAction::DPadRight;
    case PadButton::Menu:       return BackButtonAction::Menu;
    case PadButton::View:       return BackButtonAction::View;
    case PadButton::L3:         return BackButtonAction::L3;
    case PadButton::R3:         return BackButtonAction::R3;
    case PadButton::Steam:      return BackButtonAction::Guide;
    default:                    return BackButtonAction::None;
    }
}

// The settings page's row id for a button, which is also the key its binding
// travels under. Prefixed so it cannot collide with a paddle, pad or trigger
// row, and because the page's JSON reader matches a key including its quotes,
// none of these can be found inside another.
inline const char* PadButtonWireId(PadButton b) {
    static const char* ids[] = {
        "BTN_A", "BTN_B", "BTN_X", "BTN_Y",
        "BTN_LB", "BTN_RB",
        "BTN_Up", "BTN_Down", "BTN_Left", "BTN_Right",
        "BTN_Menu", "BTN_View",
        "BTN_L3", "BTN_R3",
        "BTN_Steam",
    };
    const auto i = static_cast<size_t>(b);
    return i < kPadButtonCount ? ids[i] : "";
}

// Registry value name for a button, shared by the default profile and every
// game profile so the two cannot drift apart.
inline const wchar_t* PadButtonRegistryName(PadButton b) {
    static const wchar_t* names[] = {
        L"BtnA", L"BtnB", L"BtnX", L"BtnY",
        L"BtnLB", L"BtnRB",
        L"BtnUp", L"BtnDown", L"BtnLeft", L"BtnRight",
        L"BtnMenu", L"BtnView",
        L"BtnL3", L"BtnR3",
        L"BtnSteam",
    };
    const auto i = static_cast<size_t>(b);
    return i < kPadButtonCount ? names[i] : L"";
}

struct ButtonConfig {
    BackButtonBinding map[kPadButtonCount];

    ButtonConfig() {
        for (size_t i = 0; i < kPadButtonCount; ++i)
            map[i] = BackButtonBinding::FromAction(PadButtonIdentity(static_cast<PadButton>(i)));
    }

    const BackButtonBinding& Get(PadButton b) const { return map[static_cast<size_t>(b)]; }
    BackButtonBinding&       At(PadButton b)        { return map[static_cast<size_t>(b)]; }

    // True when this button does something other than be itself. An unchanged
    // button is left to the virtual pad's own translation of the report, which
    // is exactly what it did before buttons were configurable.
    bool IsRemapped(PadButton b) const {
        return Get(b) != BackButtonBinding::FromAction(PadButtonIdentity(b));
    }

    // The binding to dispatch while this button is held, or nothing at all when
    // it has not been remapped. Reading it through here rather than the table is
    // what stops an unchanged button being sent twice — once by the virtual pad
    // and once as a binding.
    BackButtonBinding Effective(PadButton b) const {
        return IsRemapped(b) ? Get(b) : BackButtonBinding{};
    }

    bool operator==(const ButtonConfig& o) const {
        for (size_t i = 0; i < kPadButtonCount; ++i)
            if (map[i] != o.map[i]) return false;
        return true;
    }
    bool operator!=(const ButtonConfig& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Persistence
//
// Same shape as the trigger codec and for the same reason: the default profile
// lives in TrayApp and every game profile in GameProfiles, and neither can share
// the other's registry helpers. readDw(name, default) -> value; writeDw(name,
// value). A value that is absent reads back as the button's own action, which is
// how everything written before buttons were configurable behaved.
// ---------------------------------------------------------------------------

template <class ReadDw>
inline ButtonConfig ReadButtonSettings(ReadDw&& readDw) {
    ButtonConfig c;
    for (size_t i = 0; i < kPadButtonCount; ++i) {
        const auto b = static_cast<PadButton>(i);
        c.At(b) = BackButtonBinding::Unpack(
            readDw(PadButtonRegistryName(b),
                   BackButtonBinding::FromAction(PadButtonIdentity(b)).Pack()));
    }
    return c;
}

template <class WriteDw>
inline void WriteButtonSettings(const ButtonConfig& c, WriteDw&& writeDw) {
    for (size_t i = 0; i < kPadButtonCount; ++i) {
        const auto b = static_cast<PadButton>(i);
        writeDw(PadButtonRegistryName(b), c.Get(b).Pack());
    }
}
