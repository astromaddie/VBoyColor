#include "io/GameControls.h"
#include "io/Platform.h"
#include "io/Settings.h"

#include <cstdio>
#include <sstream>

namespace
{
    std::string FileOf(const std::string &game) { return game + ".buttons"; }
    constexpr const char *kHeader = "vboycolor-buttons";
} // namespace

void GameControls::SetGame(Platform &platform, const std::string &game)
{
    if (game == m_game)
        return;
    m_game = game;
    m_has = !game.empty() && Load(platform, game, m_buttons);
}

void GameControls::Reload(Platform &platform)
{
    m_has = !m_game.empty() && Load(platform, m_game, m_buttons);
}

const GameControls::Mapping &GameControls::Buttons(const AppSettings &settings) const
{
    // (not ?: - MSVC turns the arrays into pointers there)
    if (m_has)
        return m_buttons;
    return settings.vbButtons;
}

bool GameControls::Load(Platform &platform, const std::string &game, ButtonMapper::MappedButtons out[16])
{
    const std::vector<uint8_t> bytes = platform.ReadRomsFile(FileOf(game), true);
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string word;
    int version = 0;
    if (!(in >> word >> version) || word != kHeader || version != 1)
        return false;
    for (int i = 0; i < 16; ++i)
        out[i] = {};
    int bit = 0, slot = 0, set = 0, device = 0, index = 0;
    while (in >> bit >> slot >> set >> device >> index)
        if (bit >= 0 && bit < 16 && slot >= 0 && slot < 2)
            out[bit].Buttons[slot] = {set != 0, device, index};
    return true;
}

void GameControls::Save(Platform &platform, const std::string &game, const ButtonMapper::MappedButtons in[16])
{
    std::string text = std::string(kHeader) + " 1\n";
    char line[64];
    for (int bit = 0; bit < 16; ++bit)
        for (int slot = 0; slot < 2; ++slot)
        {
            const ButtonMapper::MappedButton &b = in[bit].Buttons[slot];
            std::snprintf(line, sizeof(line), "%d %d %d %d %d\n", bit, slot, b.IsSet ? 1 : 0, b.IsSet ? b.InputDevice : 0,
                          b.IsSet ? b.ButtonIndex : 0);
            text += line;
        }
    platform.WriteRomsFile(FileOf(game), true, text.data(), text.size());
}

void GameControls::Forget(Platform &platform, const std::string &game)
{
    // (no deleting through every platform's folder access: a file without the header is "none")
    const char none[] = "none\n";
    platform.WriteRomsFile(FileOf(game), true, none, sizeof(none) - 1);
}
