#pragma once

#include "input/ButtonMapping.h"

#include <string>

class Platform;
struct AppSettings;

// A game's own button mapping (Library: hold a game's card - see
// GameControlsPage), kept with the settings as <game>.buttons. A game
// without one uses the general mapping (Settings > Button mapping,
// AppSettings::vbButtons). (A game's own touch controls: TouchControls.)
class GameControls
{
public:
    // The game playing ("" none): its own mapping is read when it changes.
    void SetGame(Platform &platform, const std::string &game);
    // The mapping in force: the game's own, else the general one.
    using Mapping = ButtonMapper::MappedButtons[16];
    const Mapping &Buttons(const AppSettings &settings) const;
    // Read the game playing's mapping again (it was just changed or forgotten).
    void Reload(Platform &platform);

    // A game's own mapping into out[16]: false if it has none.
    static bool Load(Platform &platform, const std::string &game, ButtonMapper::MappedButtons out[16]);
    static void Save(Platform &platform, const std::string &game, const ButtonMapper::MappedButtons in[16]);
    // Back to the general mapping.
    static void Forget(Platform &platform, const std::string &game);

private:
    std::string m_game;
    bool m_has = false;
    ButtonMapper::MappedButtons m_buttons[16] = {};
};
