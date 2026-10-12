#pragma once
#include <map>
#include <string>
#include "TrackpadConfig.h"

// Per-game overrides, keyed by the game's id (the same opaque identity
// GameLibrary::InstalledGame::id produces — an exe path for most shortcuts
// and for an application the user picked by hand, a "steam://rungameid/N"
// URI for Steam games, an "aumid:..." for packaged apps, and a
// "dir:C:\path" install directory for a game found through a launcher's own
// records). A game with no entry here uses the global default profile
// instead — most users never create any of these.
namespace GameProfiles {

std::map<std::wstring, ControllerProfile> Load();
void Save(const std::map<std::wstring, ControllerProfile>& profiles);

// Reserved id for the one profile that is not a game at all: whatever applies
// when the foreground is the Windows shell rather than something the user
// installed (see GameLibrary::IsSystemProgram). Lives in the same map and
// goes through the same load/save/match machinery as any other per-"game"
// profile — nothing here can confuse it for a real one, since no exe path,
// steam:// URI, aumid: or dir: prefix can ever equal this literal string.
inline constexpr wchar_t kDesktopProfileId[] = L"__desktop__";

}
