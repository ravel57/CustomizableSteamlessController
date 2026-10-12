// Console diagnostic for the pieces of button remapping and circular scrolling
// that are arithmetic and persistence rather than Windows: what a button's
// default is, what a remapped one dispatches, that nothing written before
// buttons were configurable reads back as changed, and that a thumb circling the
// pad turns into the scroll it should. None of it needs a controller.
#include "app/ButtonConfig.h"
#include "app/RotaryScroll.h"
#include "app/TrackpadConfig.h"
#include <cmath>
#include <cstdio>
#include <map>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) ++g_failures;
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
}

// A fake registry: the codecs only see a name->value callable.
struct FakeRegistry {
    std::map<std::wstring, uint32_t> values;
    uint32_t Read(const wchar_t* name, uint32_t def) const {
        const auto it = values.find(name);
        return it == values.end() ? def : it->second;
    }
    void Write(const wchar_t* name, uint32_t v) { values[name] = v; }
};

// A finger at `radius` and `angle` (radians, counter-clockwise from the right,
// Y up) on the pad.
void At(int& x, int& y, double radius, double angle) {
    x = static_cast<int>(std::lround(radius * std::cos(angle)));
    y = static_cast<int>(std::lround(radius * std::sin(angle)));
}

// Total radians moved by a finger that goes `turns` times round (negative is
// clockwise) in `steps` frames at `radius`.
float Circle(RotaryTracker& t, double radius, double turns, int steps, double startAngle = 0.0) {
    float total = 0.0f;
    for (int i = 0; i <= steps; ++i) {
        int x, y;
        At(x, y, radius, startAngle + turns * 2.0 * kRotaryPi * i / steps);
        total += t.Step(x, y);
    }
    return total;
}

}  // namespace

int main() {
    using B = BackButtonBinding;

    printf("Defaults\n");
    {
        ButtonConfig c;
        bool anyRemapped = false, anyDispatched = false;
        for (size_t i = 0; i < kPadButtonCount; ++i) {
            const auto b = static_cast<PadButton>(i);
            anyRemapped   |= c.IsRemapped(b);
            anyDispatched |= c.Effective(b) != B{};
        }
        Check(!anyRemapped, "no button starts out remapped");
        Check(!anyDispatched, "an unremapped button dispatches nothing, so it is never sent twice");
        Check(c.Get(PadButton::A) == B::FromAction(BackButtonAction::A), "A is the pad's A");
        Check(c.Get(PadButton::Steam) == B::FromAction(BackButtonAction::Guide), "Steam is the Guide button");
        Check(c.Get(PadButton::Menu) == B::FromAction(BackButtonAction::Menu), "Menu is Start");
    }

    printf("\nRemapping\n");
    {
        ButtonConfig c;
        c.At(PadButton::A) = B::FromKey(0x0D);
        c.At(PadButton::Menu) = B::FromKey(0x1B);
        c.At(PadButton::B) = B::FromAction(BackButtonAction::X);
        c.At(PadButton::Y) = B::FromAction(BackButtonAction::None);
        Check(c.IsRemapped(PadButton::A) && c.Effective(PadButton::A) == B::FromKey(0x0D),
              "A bound to Enter is remapped and dispatches Enter");
        Check(c.IsRemapped(PadButton::B) && c.Effective(PadButton::B) == B::FromAction(BackButtonAction::X),
              "B bound to the gamepad's X is remapped");
        Check(c.IsRemapped(PadButton::Y) && c.Effective(PadButton::Y) == B::FromAction(BackButtonAction::None),
              "Off is a remap: the button stops doing anything");
        Check(!c.IsRemapped(PadButton::X), "a button nobody touched is not remapped");
        c.At(PadButton::A) = B::FromAction(BackButtonAction::A);
        Check(!c.IsRemapped(PadButton::A), "putting A back to A un-remaps it");
        ButtonConfig d;
        Check(!(c == d), "configs that differ compare unequal");
        d = c;
        Check(c == d, "copies compare equal");
    }

    printf("\nPersistence\n");
    {
        ButtonConfig c;
        c.At(PadButton::A) = B::FromKey(0x0D);
        c.At(PadButton::DPadUp) = B::FromKey(0x26, B::ModCtrl);
        c.At(PadButton::L3) = B::FromMouseButton(B::MouseButtonCode::Middle);
        c.At(PadButton::Steam) = B::FromAction(BackButtonAction::None);

        FakeRegistry reg;
        WriteButtonSettings(c, [&](const wchar_t* n, uint32_t v) { reg.Write(n, v); });
        const ButtonConfig back = ReadButtonSettings(
            [&](const wchar_t* n, uint32_t def) { return reg.Read(n, def); });
        Check(back == c, "remapped buttons survive the registry round trip");

        // Everything written before buttons were configurable.
        FakeRegistry empty;
        const ButtonConfig legacy = ReadButtonSettings(
            [&](const wchar_t* n, uint32_t def) { return empty.Read(n, def); });
        Check(legacy == ButtonConfig{}, "a profile with no button values reads back as every button being itself");

        std::map<std::wstring, int> names;
        bool unique = true, nonEmpty = true;
        for (size_t i = 0; i < kPadButtonCount; ++i) {
            const std::wstring n = PadButtonRegistryName(static_cast<PadButton>(i));
            unique &= (++names[n] == 1);
            nonEmpty &= !n.empty();
        }
        Check(unique && nonEmpty, "every button has its own registry value name");

        std::map<std::string, int> ids;
        bool idsUnique = true;
        for (size_t i = 0; i < kPadButtonCount; ++i)
            idsUnique &= (++ids[PadButtonWireId(static_cast<PadButton>(i))] == 1);
        Check(idsUnique, "every button has its own wire id");
    }

    printf("\nScroll style\n");
    {
        Check(ScrollStyleFromDword(0) == ScrollStyle::Swipe, "an absent value is Swipe, as every older pad was");
        Check(ScrollStyleFromDword(1) == ScrollStyle::Rotary, "1 is Rotary");
        Check(ScrollStyleFromDword(7) == ScrollStyle::Swipe, "a value from a later build degrades to Swipe");
        Check(ScrollStyleFromId("rotary") == ScrollStyle::Rotary && ScrollStyleFromId("") == ScrollStyle::Swipe,
              "wire ids round-trip, and a missing one is Swipe");

        ControllerProfile fresh;
        Check(fresh.leftPad.mode == TrackpadMode::ScrollWheel && fresh.leftPad.scrollStyle == ScrollStyle::Rotary,
              "a fresh profile's left pad scrolls by circling");
        Check(fresh.leftPad.click == B::FromAction(BackButtonAction::RightMouseButton),
              "its click is a right click, like Steam's desktop layout");
        Check(fresh.rightPad.mode == TrackpadMode::MousePointer
              && fresh.rightPad.click == B::FromAction(BackButtonAction::LeftMouseButton),
              "the right pad points and left-clicks");
        Check(fresh.buttons == ButtonConfig{}, "buttons start as the gamepad they are");
        Check(fresh.leftTrigger.mode == TriggerMode::Standard, "triggers start as analog triggers");

        ControllerProfile other = fresh;
        Check(other == fresh, "identical profiles compare equal");
        other.buttons.At(PadButton::A) = B::FromKey(0x0D);
        Check(other != fresh, "a remapped button makes a profile differ");
        other = fresh;
        other.leftPad.scrollStyle = ScrollStyle::Swipe;
        Check(other != fresh, "so does a pad's scroll style");
    }

    printf("\nCircular scrolling\n");
    {
        RotaryTracker t;
        const double r = 20000;

        float ccw = Circle(t, r, 1.0, 240);
        Check(std::fabs(ccw - 2.0f * kRotaryPi) < 0.05f, "a full counter-clockwise lap is about 2*pi");
        t.Reset();
        float cw = Circle(t, r, -1.0, 240);
        Check(std::fabs(cw + 2.0f * kRotaryPi) < 0.05f, "a full clockwise lap is about -2*pi");

        // Crossing the atan2 wrap (+pi to -pi) must not read as a lap.
        t.Reset();
        float across = Circle(t, r, 0.25, 30, kRotaryPi - 0.2);
        Check(std::fabs(across - 0.5f * kRotaryPi) < 0.05f, "going over the +pi/-pi seam is not a jump");

        // Wheel units for a full lap at the default speed.
        const float units = 2.0f * kRotaryPi * kRotaryWheelUnitsPerRadian;
        Check(std::fabs(units - kRotaryNotchesPerTurn * 120.0f) < 0.5f,
              "a lap at 100% is the configured number of wheel notches");

        // Nothing to start from.
        t.Reset();
        int x, y;
        At(x, y, r, 1.0);
        Check(t.Step(x, y) == 0.0f, "the first frame of a touch moves nothing");
        Check(t.HasReference(), "and leaves a reference to measure from");

        // The middle of the pad is not an angle worth trusting.
        At(x, y, kRotaryDeadRadius - 500, 2.0);
        Check(t.Step(x, y) == 0.0f && !t.HasReference(), "entering the dead radius drops the reference");
        At(x, y, r, 3.0);
        Check(t.Step(x, y) == 0.0f, "coming back out does not scroll by the angle swept inside");
        At(x, y, r, 3.1);
        Check(std::fabs(t.Step(x, y) - 0.1f) < 0.01f, "and scrolling resumes from the new reference");

        // A lift and re-placement the touch bit never reported.
        t.Reset();
        At(x, y, r, 0.0);  t.Step(x, y);
        At(x, y, r, 2.5);
        Check(t.Step(x, y) == 0.0f, "a jump no finger can make is ignored");
        At(x, y, r, 2.6);
        Check(std::fabs(t.Step(x, y) - 0.1f) < 0.01f, "and the frame after it scrolls normally from the new place");

        // Radius does not matter, only angle: a lap near the rim and a lap near
        // the dead radius scroll the same.
        t.Reset();
        const float rim = Circle(t, 32000, 1.0, 240);
        t.Reset();
        const float inner = Circle(t, kRotaryDeadRadius + 1500, 1.0, 240);
        Check(std::fabs(rim - inner) < 0.05f, "a lap scrolls the same wherever on the pad it is made");
    }

    printf("\n%s (%d failure(s))\n", g_failures ? "FAILED" : "All checks passed", g_failures);
    return g_failures ? 1 : 0;
}
