#pragma once

#include "gfx/UiFontManager.h" // UiFontHandle
#include "gfx/UiIconSet.h"

#include <functional>
#include <string>

class Emulator;
class AppMenu;
struct AppSettings;
class Platform;
class ThumbnailLibrary;
class Updater;
class GameControls;

enum class ButtonMappingProfile
{
    Vr,
    Desktop,
    Phone // (a flat touch screen - platform/android/PhoneMain.cpp)
};

// Shared read-only resources every menu page needs, loaded once by
// AppMenu::Initialize and handed to each page's Init() as a single bundle -
// keeps MenuPage::Init from growing a new parameter every time a page needs
// another shared font/atlas.
struct UiMenuResources
{
    // See AppMenuLayout.h's font sizes. Regular unless *Bold.
    UiFontHandle titleFont;   // bold
    UiFontHandle bodyFont;
    UiFontHandle bodyBoldFont;
    UiFontHandle cardFont;
    UiFontHandle cardBoldFont;
    UiFontHandle captionFont;
    UiFontHandle captionBoldFont;
    UiFontHandle smallFont;   // bold, kSmallFontSize
    const UiIconSet *icons = nullptr;
    // Not const - LibraryPage's press callback calls LoadRom on it, and
    // pages call Hide() on the AppMenu below to resume gameplay.
    Emulator *emulator = nullptr;
    AppMenu *appMenu = nullptr;
    // Not const - SettingsPage/MoveScreenPage/the button-map pages all write
    // through this directly (single shared instance, see AppSettings' own
    // doc comment) - no per-page copies to keep in sync.
    AppSettings *settings = nullptr;
    ButtonMappingProfile buttonMappingProfile = ButtonMappingProfile::Vr;
    Platform *platform = nullptr;
    // The headset can show the room around the screen (AppSettings::passthrough).
    bool passthroughSupported = false;
    // The library's games and their thumbnails (owned by AppMenu).
    ThumbnailLibrary *thumbnails = nullptr;
    // Is there a newer VBoy Color (owned by AppMenu - see io/Updater.h).
    Updater *updater = nullptr;
    // The phone app: Settings > Touch controls > Edit (the app closes the
    // menu and lets the player move the on-screen controls) - game: a game's
    // own layout (GameControlsPage), "" the general one; and a game's own
    // layout forgotten. Set by the app (AppMenu::SetEditTouchControls) -
    // empty elsewhere.
    std::function<void(const std::string &game)> editTouchControls;
    std::function<void(const std::string &game)> forgetTouchControls;
    // The games' own button mappings (owned by AppMenu - see io/GameControls.h).
    GameControls *gameControls = nullptr;
};
