#pragma once

#include <string>

// Settings > Folders: the four kinds of files VBoy Color keeps, each in a
// folder the player can choose. By default they all live in the games folder
// as before: in-game saves (.srm) next to the ROMs, save states, settings
// and per-game colors in its "States" subfolder.
//
// Games is everything else the app reads or writes next to the ROMs: the
// ROMs themselves, color packs (.vbcp) and their paintings (colorpacks/),
// captures/ and recordings/.
enum class DataKind
{
    Games = 0,
    Saves = 1,       // in-game saves (.srm)
    States = 2,      // save states and their pictures (.state, .stateimg, .statepng + slot)
    Settings = 3,    // settings.dat (and the PC VR app's own), <game>.colors, the library's lists and thumbnails
    Screenshots = 4, // "screenshots/<game> NNN.png" (Emulator::SaveScreenshot) - the games folder's "screenshots" by default
};
inline constexpr int kDataKindCount = 5;

// A screenshot's file name as Platform::WriteRomsFile gets it: this prefix,
// then the name - which goes into the Screenshots folder, wherever it is.
inline constexpr const char *kScreenshotsPrefix = "screenshots/";

// The name a file has in its kind's folder: a screenshot's without
// kScreenshotsPrefix, any other's as it is.
std::string NameInFolder(const std::string &fileName, DataKind kind);

// Which kind a file is - fileName and inStatesDir as Platform::WriteRomsFile
// gets them (inStatesDir: the games folder's "States" subfolder by default).
DataKind DataKindOf(const std::string &fileName, bool inStatesDir);

// Where a kind lives by default: its subfolder of the games folder ("" for
// the games folder itself).
inline const char *DefaultSubfolder(DataKind kind)
{
    return kind == DataKind::States || kind == DataKind::Settings ? "States" : kind == DataKind::Screenshots ? "screenshots" : "";
}

// Where the menu says a kind lives by default ("In the games folder's States").
inline const char *DefaultPlaceName(DataKind kind)
{
    switch (kind)
    {
    case DataKind::Saves:
        return "In the games folder";
    case DataKind::Screenshots:
        return "In the games folder's screenshots";
    default:
        return "In the games folder's States";
    }
}

// Its "back to the default place" choice.
inline const char *BackToDefaultName(DataKind kind)
{
    switch (kind)
    {
    case DataKind::Saves:
        return "Back in the games folder";
    case DataKind::Screenshots:
        return "Back in the games folder's screenshots";
    default:
        return "Back in the games folder's States";
    }
}

// How the menu names a kind.
inline const char *DataKindName(DataKind kind)
{
    switch (kind)
    {
    case DataKind::Games:
        return "Games";
    case DataKind::Saves:
        return "Saved games";
    case DataKind::States:
        return "Save states";
    case DataKind::Screenshots:
        return "Screenshots";
    default:
        return "Settings";
    }
}
