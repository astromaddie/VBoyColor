#include "menu/pages/SupportPage.h"
#include "emu/AudioOutput.h"
#include "emu/Emulator.h"
#include "io/Platform.h"
#include "io/Settings.h"
#include "menu/AppMenu.h"
#include "menu/pages/AppMenuLayout.h"
#include "menu/pages/YunoSprites.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    // The game's own pixels: the square is kGame of them across, whatever its
    // size in menu units.
    constexpr float kGame = 190.0f;
    constexpr float kGroundY = 152.0f; // the ground's top
    constexpr float kHillY = kGroundY + 1.0f; // the mountains' feet (behind the ground's edge) - and Nucita's, far away
    constexpr float kYunoX = 20.0f;   // (his sprite's left: the tail - his body from +11 to +34)
    constexpr float kGravity = 1150.0f, kJumpSpeed = 340.0f;
    constexpr float kObstacleScale = 2.0f; // (the rocks and bushes, twice their drawn size)
    constexpr float kTileW = 24.0f, kEdgeW = 15.0f;

    // The Virtual Boy's four shades of red (sRGB), black to brightest.
    constexpr uint8_t kShades[4] = {0, 118, 186, 255};
    constexpr XrColor4f kRed = {1.0f, 0.12f, 0.12f, 1.0f};
    constexpr XrColor4f kDimRed = {0.62f, 0.06f, 0.06f, 1.0f};

    XrColor4f WithAlpha(XrColor4f c, float a)
    {
        c.a *= a;
        return c;
    }

    float TextY(UiRenderer &ui, UiFontHandle font, float centerY)
    {
        return centerY - ui.GetFontPHeight(font) / 2.0f - ui.GetFontPStart(font);
    }

    std::vector<std::string> Wrap(UiRenderer &ui, UiFontHandle font, const std::string &text, float width)
    {
        std::vector<std::string> lines;
        std::string line, word;
        auto flushWord = [&]()
        {
            if (word.empty())
                return;
            const std::string tryLine = line.empty() ? word : line + " " + word;
            if (!line.empty() && ui.GetTextWidth(font, tryLine) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else
                line = tryLine;
            word.clear();
        };
        for (char c : text)
        {
            if (c == ' ')
                flushWord();
            else
                word += c;
        }
        flushWord();
        if (!line.empty())
            lines.push_back(line);
        return lines;
    }

    // "00042"
    std::string Digits(int n)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%05d", std::clamp(n, 0, 99999));
        return buf;
    }
} // namespace

// -----------------------------------------------------------------------
// Setting up

void SupportPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_res = &resources;
    m_settings = resources.settings;
    m_platform = resources.platform;
    m_emulator = resources.emulator;
    m_profile = resources.buttonMappingProfile;
    BuildAtlas(ui);
    BuildSounds();
    StopGame();
}

int SupportPage::SpriteIndex(const char *name) const
{
    for (int i = 0; i < kYunoSpriteCount; ++i)
        if (std::strcmp(kYunoSprites[i].name, name) == 0)
            return i;
    return 0;
}

void SupportPage::BuildAtlas(UiRenderer &ui)
{
    // Shelves, a pixel of clear space around each picture.
    constexpr int kAtlasW = 128;
    struct Place
    {
        int x, y;
    };
    std::vector<Place> places(kYunoSpriteCount);
    int x = 1, y = 1, shelf = 0;
    for (int i = 0; i < kYunoSpriteCount; ++i)
    {
        const YunoSprite &s = kYunoSprites[i];
        if (x + s.width + 1 > kAtlasW)
        {
            x = 1;
            y += shelf + 1;
            shelf = 0;
        }
        places[i] = {x, y};
        x += s.width + 1;
        shelf = std::max(shelf, s.height);
    }
    const int atlasH = y + shelf + 1;
    std::vector<uint8_t> bgra(static_cast<size_t>(kAtlasW) * atlasH * 4, 0);
    m_sprites.resize(kYunoSpriteCount);
    for (int i = 0; i < kYunoSpriteCount; ++i)
    {
        const YunoSprite &s = kYunoSprites[i];
        for (int row = 0; row < s.height; ++row)
            for (int col = 0; col < s.width; ++col)
            {
                const char c = s.rows[row][col];
                if (c < '0' || c > '3')
                    continue;
                uint8_t *p = &bgra[(static_cast<size_t>(places[i].y + row) * kAtlasW + places[i].x + col) * 4];
                p[2] = kShades[c - '0'];
                p[3] = 255;
            }
        Sprite &out = m_sprites[i];
        out.w = s.width;
        out.h = s.height;
        out.u0 = static_cast<float>(places[i].x) / kAtlasW;
        out.v0 = static_cast<float>(places[i].y) / atlasH;
        out.u1 = static_cast<float>(places[i].x + s.width) / kAtlasW;
        out.v1 = static_cast<float>(places[i].y + s.height) / atlasH;
    }
    m_atlas = ui.CreateStreamingImage(kAtlasW, static_cast<uint32_t>(atlasH), VK_FORMAT_B8G8R8A8_SRGB);
    ui.UpdateStreamingImage(m_atlas, bgra.data(), bgra.size());

    for (int i = 0; i < 6; ++i)
        m_sRun[i] = SpriteIndex(("yunoRun" + std::to_string(i)).c_str());
    m_sJump = m_sRun[0]; // (the leap, legs stretched out)
    m_sSit = SpriteIndex("yunoSit");
    m_sGround = SpriteIndex("ground");
    m_sFill = SpriteIndex("fill");
    m_sEdgeL = SpriteIndex("gapL");
    m_sEdgeR = SpriteIndex("gapR");
    m_sNucitaSit = SpriteIndex("nucitaSit");
    m_sNucitaBlink = SpriteIndex("nucitaBlink");
    for (int i = 0; i < 4; ++i)
        m_sNucitaWalk[i] = SpriteIndex(("nucitaWalk" + std::to_string(i)).c_str());
    m_sNucitaHop[0] = SpriteIndex("nucitaJump0");
    m_sNucitaHop[1] = SpriteIndex("nucitaJump1");
    for (const char *name : {"rock1", "rock2", "rock3", "rock4", "bush1", "bush2", "grass1", "grass2"})
        m_obstacleSprites.push_back(SpriteIndex(name));
    for (const char *name : {"cloud1", "cloud2", "cloud3", "cloud4"})
        m_cloudSprites.push_back(SpriteIndex(name));
    for (int i = 0; i < 8; ++i)
        m_mountainSprites.push_back(SpriteIndex(("mountain" + std::to_string(i)).c_str()));
}

void SupportPage::BuildSounds()
{
    // A "boing" a la a chiptune: a square wave sweeping up, fading out - and
    // a falling one for the end of a run. Quiet: it's the menu.
    constexpr float kRate = static_cast<float>(AudioOutput::kSampleRate);
    auto sweep = [&](float seconds, float f0, float f1, float volume, bool square)
    {
        std::vector<int16_t> out;
        const int frames = static_cast<int>(seconds * kRate);
        out.reserve(static_cast<size_t>(frames) * 2);
        float phase = 0.0f;
        for (int i = 0; i < frames; ++i)
        {
            const float t = static_cast<float>(i) / frames;
            const float f = f0 * std::pow(f1 / f0, t);
            phase += f / kRate;
            phase -= std::floor(phase);
            const float wave = square ? (phase < 0.5f ? 1.0f : -1.0f) : (4.0f * std::abs(phase - 0.5f) - 1.0f);
            const float attack = std::min(1.0f, i / (0.004f * kRate));
            const float env = attack * (1.0f - t) * (1.0f - t);
            const int16_t v = static_cast<int16_t>(wave * env * volume * 32767.0f);
            out.push_back(v);
            out.push_back(v);
        }
        return out;
    };
    m_jumpSound = sweep(0.11f, 320.0f, 760.0f, 0.12f, true);
    m_overSound = sweep(0.32f, 420.0f, 90.0f, 0.18f, false);
}

void SupportPage::PlaySound(const std::vector<int16_t> &sound)
{
    if (m_emulator && !sound.empty())
        m_emulator->PlayMenuSound(sound.data(), sound.size() / 2);
}

// -----------------------------------------------------------------------
// Focus and input

void SupportPage::SetFocused(bool focused)
{
    m_focused = focused;
    if (!focused)
    {
        StopGame(); // (out of the page: the run ends)
        m_buttonHover = false;
        if (m_urlPending && m_platform)
            m_platform->OpenUrl(kSponsorUrl);
        m_urlPending = false;
        m_pressTime = -1.0f;
    }
}

void SupportPage::PressButton()
{
    // A little squish and a ring, then the page (a moment later, so the click shows).
    m_pressTime = 0.0f;
    m_urlPending = true;
}

void SupportPage::ResetSelection()
{
    m_focus = Focus::Button;
    StopGame();
}

void SupportPage::OnShow() { StopGame(); }

std::vector<MenuHint> SupportPage::Hints() const
{
    if (m_focus == Focus::Game)
        return {{UiIconId::ButtonA, m_state == State::Running ? "Jump" : "Play"}, {UiIconId::ButtonB, "Back"}};
    return {{UiIconId::ButtonA, "Gift me a coffee"}, {UiIconId::ButtonB, "Back"}};
}

void SupportPage::SubmitKey(int key)
{
    if (key == 32 && m_focused) // Space
        Activate();
}

void SupportPage::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    using namespace ButtonMapper;
    auto fresh = [&](uint32_t device, uint32_t button)
    { return (buttonState[device] & ButtonMapping[button]) && !(lastButtonState[device] & ButtonMapping[button]); };
    auto dir = [&](uint32_t pad, uint32_t stick)
    {
        return fresh(DeviceGamepad, pad) || fresh(DeviceGamepad, stick) || fresh(DeviceLeftTouch, pad) ||
               fresh(DeviceRightTouch, pad);
    };
    const bool up = dir(EmuButton_Up, EmuButton_LeftStickUp), down = dir(EmuButton_Down, EmuButton_LeftStickDown);
    const bool left = dir(EmuButton_Left, EmuButton_LeftStickLeft), right = dir(EmuButton_Right, EmuButton_LeftStickRight);
    const bool a = fresh(DeviceGamepad, EmuButton_A) || fresh(DeviceRightTouch, EmuButton_A);
    // (B: on a gamepad it jumps too, while the game has the focus; Esc, a
    // headset's B and a phone's Back always go back)
    const bool padB = fresh(DeviceGamepad, EmuButton_B), otherB = fresh(DeviceRightTouch, EmuButton_B);
    const bool xy = fresh(DeviceGamepad, EmuButton_X) || fresh(DeviceGamepad, EmuButton_Y) ||
                    fresh(DeviceLeftTouch, EmuButton_X) || fresh(DeviceLeftTouch, EmuButton_Y) ||
                    fresh(DeviceRightTouch, EmuButton_X) || fresh(DeviceRightTouch, EmuButton_Y);

    auto back = [this]()
    {
        StopGame();
        if (m_menu.BackPress)
        {
            const auto press = m_menu.BackPress;
            press();
        }
    };
    auto leftEdge = [this]()
    {
        StopGame();
        if (m_menu.LeftEdge)
        {
            const auto edge = m_menu.LeftEdge;
            edge();
        }
    };

    // (a key or button moving the focus shows the highlight again)
    if (up || down || left || right || a || padB || otherB || xy)
        m_mouseLeft = false;
    // (the pointer went off the page - to the sidebar, or out of the menu: not over the button any more)
    if (!m_hoverSeen)
    {
        if (m_buttonHover)
            m_mouseLeft = true;
        m_buttonHover = false;
    }
    m_hoverSeen = false;
    if (m_pressTime >= 0.0f)
    {
        m_pressTime += deltaSeconds;
        if (m_urlPending && m_pressTime >= 0.2f)
        {
            m_urlPending = false;
            if (m_platform)
                m_platform->OpenUrl(kSponsorUrl);
        }
        if (m_pressTime > 0.45f)
            m_pressTime = -1.0f;
    }

    if (m_focus == Focus::Button)
    {
        if (a)
            PressButton();
        else if (down || right)
            m_focus = Focus::Game;
        else if (left)
            leftEdge();
        else if (padB || otherB)
            back();
    }
    else
    {
        if (a || xy || padB)
            Activate();
        else if (otherB)
            back();
        else if (up)
        {
            StopGame();
            m_focus = Focus::Button;
        }
        else if (left)
            leftEdge();
    }

    // The game's own clock, evened out: a frame's measured time wobbles by a
    // few milliseconds around the screen's refresh (when the loop got to run,
    // not when the picture showed), and stepping by it as it is makes the
    // scrolling stutter now and then. Every frame steps by their average - a
    // frame or two the screen missed included (the game waits for it rather
    // than jumping ahead, which shows as a jerk); only a longer stall counts
    // as it is.
    const float dt = std::min(deltaSeconds, 0.05f);
    if (m_smoothDt <= 0.0f)
        m_smoothDt = dt;
    const float step = dt < m_smoothDt * 2.6f ? m_smoothDt : dt;
    if (dt < m_smoothDt * 1.5f) // (the average follows steady frames only)
        m_smoothDt = m_smoothDt * 0.95f + dt * 0.05f;
    StepGame(step);
}

void SupportPage::HandlePointer(const MenuPointer &pointer)
{
    const float x = pointer.x, y = pointer.y;
    const bool inButton = x >= m_buttonX && x <= m_buttonX + m_buttonW && y >= m_buttonY && y <= m_buttonY + m_buttonH;
    const bool inGame = x >= m_gameX && x <= m_gameX + m_gameSize && y >= m_gameY && y <= m_gameY + m_gameSize;
    const bool inJump = m_jumpR > 0.0f && std::hypot(x - m_jumpX, y - m_jumpY) <= m_jumpR + 2.0f;
    m_hoverSeen = true;
    if (pointer.moved && m_buttonHover && !inButton)
        m_mouseLeft = true; // (moved off it: no highlight)
    m_buttonHover = inButton;
    // (hovering moves the focus - but never out of a run)
    if (pointer.moved && inButton && m_state != State::Running)
    {
        StopGame();
        m_focus = Focus::Button;
        m_mouseLeft = false;
    }
    if (pointer.clicked && inButton)
    {
        StopGame();
        m_focus = Focus::Button;
        PressButton();
    }
    // The game jumps the moment it's pressed (a mouse, a finger, a laser's trigger).
    if (pointer.pressed && (inGame || inJump))
        Activate();
}

// -----------------------------------------------------------------------
// The game

void SupportPage::Activate()
{
    m_focus = Focus::Game;
    switch (m_state)
    {
    case State::Title:
        StartRun();
        break;
    case State::Running:
        if (m_onGround)
            Jump();
        else
            m_jumpBuffer = 0.12f;
        break;
    case State::GameOver:
        if (m_overTime > 0.45f)
            StartRun();
        break;
    case State::Falling:
        break;
    }
}

void SupportPage::StopGame()
{
    m_state = State::Title;
    m_scroll = 0.0f;
    m_speed = 0.0f;
    m_pieces.clear();
    m_obstacles.clear();
    m_genX = 0.0f;
    m_nextEventX = 1e9f; // (the title: just ground)
    m_yunoY = kGroundY;
    m_yunoVy = 0.0f;
    m_onGround = true;
    m_nucita = NucitaMode::Away;
    m_titleTime = 0.0f;
    Generate();
    GenerateMountains();
    if (m_clouds.empty())
    {
        std::uniform_real_distribution<float> cy(74.0f, 98.0f);
        for (float cx : {24.0f, 96.0f, 156.0f})
            m_clouds.push_back({cx, cy(m_rng), m_cloudSprites[m_rng() % m_cloudSprites.size()]});
    }
}

void SupportPage::StartRun()
{
    m_state = State::Running;
    m_scroll = 0.0f;
    m_speed = 120.0f;
    m_distance = 0.0f;
    m_runTime = 0.0f;
    m_yunoY = kGroundY;
    m_yunoVy = 0.0f;
    m_onGround = true;
    m_jumpBuffer = 0.0f;
    m_pieces.clear();
    m_obstacles.clear();
    m_genX = 0.0f;
    m_nextEventX = kGame + 40.0f; // (the first one past the screen's edge)
    m_newBest = false;
    m_nucita = NucitaMode::Away;
    m_nucitaNext = 3.0f;
    m_lastHundred = 0;
    Generate();
}

void SupportPage::Jump()
{
    m_onGround = false;
    m_yunoVy = -kJumpSpeed;
    m_jumpBuffer = 0.0f;
    PlaySound(m_jumpSound);
}

void SupportPage::Generate()
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    while (m_genX < m_scroll + kGame + 48.0f)
    {
        if (m_genX + kTileW <= m_nextEventX)
        {
            m_pieces.push_back({m_genX, kTileW, PieceKind::Ground});
            m_genX += kTileW;
            continue;
        }
        const float after = m_speed * (0.8f + 0.9f * unit(m_rng)) + 30.0f;
        if (m_distance > 250.0f && unit(m_rng) < 0.3f)
        {
            // A gap: the ground ends, a drop, the ground again.
            const float w = std::round(16.0f + m_speed * 0.15f + 6.0f * unit(m_rng));
            m_pieces.push_back({m_genX, kEdgeW, PieceKind::EdgeLeft});
            m_pieces.push_back({m_genX + kEdgeW, w, PieceKind::Gap});
            m_pieces.push_back({m_genX + kEdgeW + w, kEdgeW, PieceKind::EdgeRight});
            m_genX += kEdgeW * 2.0f + w;
        }
        else
        {
            // Something to jump over: the big rock and the tall grass once she's warmed up.
            const size_t choices = m_distance > 600.0f ? m_obstacleSprites.size() : m_obstacleSprites.size() - 1;
            int sprite = m_obstacleSprites[m_rng() % choices];
            if (m_distance < 600.0f && sprite == SpriteIndex("rock3"))
                sprite = SpriteIndex("rock2");
            m_pieces.push_back({m_genX, kTileW, PieceKind::Ground});
            const float w = m_sprites[sprite].w * kObstacleScale;
            m_obstacles.push_back({m_genX + std::max(0.0f, (kTileW - w) / 2.0f), sprite});
            m_genX += kTileW;
        }
        m_nextEventX = m_genX + after;
    }
    // (what scrolled off)
    while (!m_pieces.empty() && m_pieces.front().x + m_pieces.front().w < m_scroll - 8.0f)
        m_pieces.erase(m_pieces.begin());
    while (!m_obstacles.empty() && m_obstacles.front().x + 72.0f < m_scroll)
        m_obstacles.erase(m_obstacles.begin());
}

void SupportPage::GenerateMountains()
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    while (m_mountainGenX < m_mountainScroll + kGame + 8.0f)
    {
        int sprite;
        do
            sprite = m_mountainSprites[m_rng() % m_mountainSprites.size()];
        while (sprite == m_lastMountain && m_mountainSprites.size() > 1);
        m_lastMountain = sprite;
        // (side by side, a little overlap or a little gap)
        const float x = std::round(m_mountainGenX - 10.0f + 26.0f * unit(m_rng));
        m_mountains.push_back({x, sprite});
        m_mountainGenX = x + m_sprites[sprite].w;
    }
    while (!m_mountains.empty() && m_mountains.front().x + m_sprites[m_mountains.front().sprite].w < m_mountainScroll - 4.0f)
        m_mountains.erase(m_mountains.begin());
}

void SupportPage::EndRun(bool fell)
{
    (void)fell;
    m_state = State::GameOver;
    m_overTime = 0.0f;
    PlaySound(m_overSound);
    if (m_settings && Score() > m_settings->yunoBest)
    {
        m_settings->yunoBest = Score();
        m_newBest = true;
        if (m_platform)
            m_settings->Save(*m_platform);
    }
}

void SupportPage::StepGame(float dt)
{
    m_blinkTime += dt;
    if (m_state == State::Running)
    {
        m_animTime += dt; // (his legs stop when the run does)
        m_runPhase += dt * (6.0f + m_speed / 40.0f);
    }
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    // The clouds drift (a little faster while he runs).
    const float cloudSpeed = 4.0f + (m_state == State::Running ? m_speed * 0.15f : 0.0f);
    if (m_state != State::GameOver && m_state != State::Falling)
    {
        for (Cloud &c : m_clouds)
            c.x -= cloudSpeed * dt;
        m_clouds.erase(std::remove_if(m_clouds.begin(), m_clouds.end(), [](const Cloud &c) { return c.x < -24.0f; }),
                       m_clouds.end());
        m_nextCloud -= dt * (m_state == State::Running ? 1.0f + m_speed / 120.0f : 1.0f);
        if (m_nextCloud <= 0.0f)
        {
            m_clouds.push_back({kGame + 2.0f, 74.0f + 24.0f * unit(m_rng), m_cloudSprites[m_rng() % m_cloudSprites.size()]});
            m_nextCloud = 2.5f + 4.0f * unit(m_rng);
        }
    }

    if (m_state == State::Title)
    {
        m_titleTime += dt;
        return;
    }
    if (m_state == State::GameOver)
    {
        m_overTime += dt;
        return;
    }
    if (m_state == State::Falling)
    {
        m_yunoVy += kGravity * dt;
        m_yunoY += m_yunoVy * dt;
        if (m_yunoY > kGame + 40.0f)
            EndRun(true);
        return;
    }

    // Running: faster and faster.
    m_runTime += dt;
    m_speed = std::min(240.0f, 120.0f + 4.0f * m_runTime);
    m_scroll += m_speed * dt;
    m_distance += m_speed * dt;
    m_mountainScroll += m_speed * 0.2f * dt;
    Generate();
    GenerateMountains();

    // Over a gap? (both feet over it)
    auto overGap = [this]()
    {
        const float footL = m_scroll + kYunoX + 14.0f, footR = m_scroll + kYunoX + 30.0f;
        for (const Piece &p : m_pieces)
            if (p.kind == PieceKind::Gap && footL >= p.x - 2.0f && footR <= p.x + p.w + 2.0f)
                return true;
        return false;
    };
    m_jumpBuffer -= dt;
    if (!m_onGround)
    {
        m_yunoVy += kGravity * dt;
        m_yunoY += m_yunoVy * dt;
        if (m_yunoY >= kGroundY && m_yunoVy > 0.0f)
        {
            if (overGap())
                m_state = State::Falling;
            else
            {
                m_yunoY = kGroundY;
                m_yunoVy = 0.0f;
                m_onGround = true;
                if (m_jumpBuffer > 0.0f)
                    Jump();
            }
        }
    }
    else if (overGap())
    {
        m_onGround = false;
        m_yunoVy = 0.0f;
        m_state = State::Falling;
    }

    // Bumped into something?
    const float yx0 = m_scroll + kYunoX + 13.0f, yx1 = m_scroll + kYunoX + 32.0f;
    const float yy0 = m_yunoY - 19.0f, yy1 = m_yunoY - 1.0f;
    for (const Obstacle &o : m_obstacles)
    {
        const Sprite &s = m_sprites[o.sprite];
        const float ox0 = o.x + 2.0f, ox1 = o.x + s.w * kObstacleScale - 2.0f;
        const float oy0 = kGroundY - s.h * kObstacleScale + 3.0f, oy1 = kGroundY;
        if (yx0 < ox1 && yx1 > ox0 && yy0 < oy1 && yy1 > oy0)
        {
            EndRun(false);
            return;
        }
    }

    // Nucita, on the hills now and then - and a hop every hundred points.
    if (Score() / 100 > m_lastHundred)
    {
        m_lastHundred = Score() / 100;
        m_nucitaHop = 0.5f;
    }
    m_nucitaHop = std::max(0.0f, m_nucitaHop - dt);
    m_nucitaTimer += dt;
    switch (m_nucita)
    {
    case NucitaMode::Away:
        m_nucitaNext -= dt;
        if (m_nucitaNext <= 0.0f)
        {
            m_nucita = unit(m_rng) < 0.6f ? NucitaMode::Walking : NucitaMode::Sitting;
            m_nucitaX = kGame + 4.0f;
            m_nucitaTimer = 0.0f;
        }
        break;
    case NucitaMode::Walking:
    case NucitaMode::Sitting:
        m_nucitaX -= (m_speed * 0.35f - (m_nucita == NucitaMode::Walking ? 18.0f : 0.0f)) * dt;
        if (m_nucitaX < -20.0f)
        {
            m_nucita = NucitaMode::Away;
            m_nucitaNext = 8.0f + 9.0f * unit(m_rng);
        }
        break;
    }
}

// -----------------------------------------------------------------------
// Drawing

void SupportPage::DrawCoffeeCup(UiRenderer &ui, float x, float y, float s, const XrColor4f &color)
{
    XrColor4f steam = color;
    steam.a *= 0.75f;
    ui.DrawQuadRounded(x + 0.26f * s, y + 0.0f * s, 0.09f * s, 0.24f * s, steam, 0.045f * s);
    ui.DrawQuadRounded(x + 0.46f * s, y + 0.06f * s, 0.09f * s, 0.2f * s, steam, 0.045f * s);
    ui.DrawQuadRounded(x + 0.08f * s, y + 0.34f * s, 0.66f * s, 0.52f * s, color, 0.14f * s);
    ui.DrawQuad(x + 0.08f * s, y + 0.34f * s, 0.66f * s, 0.14f * s, color);
    // the handle
    ui.DrawQuadRounded(x + 0.68f * s, y + 0.42f * s, 0.24f * s, 0.09f * s, color, 0.04f * s);
    ui.DrawQuadRounded(x + 0.84f * s, y + 0.42f * s, 0.09f * s, 0.3f * s, color, 0.04f * s);
    ui.DrawQuadRounded(x + 0.68f * s, y + 0.63f * s, 0.24f * s, 0.09f * s, color, 0.04f * s);
    // the saucer
    ui.DrawQuadRounded(x + 0.0f * s, y + 0.9f * s, 0.86f * s, 0.1f * s, color, 0.05f * s);
}

void SupportPage::Layout(UiRenderer &ui)
{
    if (m_laidOut || !m_res)
        return;
    m_laidOut = true;
    const UiFontHandle body = m_res->bodyFont;
    constexpr float kPad = 8.0f, kLine = 10.5f;
    m_cardLines = Wrap(ui,
                       body,
                       "Hey there! VBoy Color is free, and it always will be. If you're enjoying it and want to help "
                       "it keep growing, you can gift me a coffee. Either way, keep being awesome!",
                       kContentWidth - kPad * 2.0f);
    m_buttonH = 16.0f;
    m_buttonW = ui.GetTextWidth(m_res->bodyBoldFont, "Gift me a coffee") + 34.0f;
    m_buttonX = kContentX + kPad;
    m_buttonY = kContentTop + kPad + m_cardLines.size() * kLine + 4.0f;
    m_cardH = m_buttonY + m_buttonH + kPad - kContentTop;

    // The game: a square at the bottom right; its words beside it.
    m_gameY = kContentTop + m_cardH + kGroupGap;
    m_gameSize = std::floor(kContentBottom - m_gameY);
    m_gameX = kContentRight - m_gameSize;
    const float sideW = m_gameX - kContentX - 10.0f;
    m_sideLines = Wrap(ui, body, "A little thank-you for stopping by: help my cat Yuno run as far as he can.",
                       sideW);
    if (m_profile == ButtonMappingProfile::Phone)
    {
        m_jumpR = 15.0f;
        m_jumpX = kContentX + sideW / 2.0f;
        m_jumpY = kContentBottom - m_jumpR - 4.0f;
    }
}

void SupportPage::Draw(UiRenderer &ui, int transitionDirX, int transitionDirY, float moveProgress, float moveDist,
                       float fadeProgress)
{
    MenuPage::Draw(ui, transitionDirX, transitionDirY, moveProgress, moveDist, fadeProgress); // (the title)
    Layout(ui);
    const float ox = transitionDirX * moveProgress * moveDist, oy = transitionDirY * moveProgress * moveDist;
    DrawCard(ui, ox, oy, fadeProgress);
    DrawSide(ui, ox, oy, fadeProgress);
    DrawGame(ui, ox, oy, fadeProgress);
}

void SupportPage::DrawCard(UiRenderer &ui, float ox, float oy, float alpha)
{
    constexpr float kPad = 8.0f, kLine = 10.5f;
    ui.DrawQuadRounded(kContentX + ox, kContentTop + oy, kContentWidth, m_cardH, WithAlpha(kMenuCardColor, alpha), kCardRadius);
    const UiFontHandle body = m_res->bodyFont, bold = m_res->bodyBoldFont;
    float y = kContentTop + kPad;
    for (const std::string &line : m_cardLines)
    {
        ui.DrawText(body, line, kContentX + kPad + ox, TextY(ui, body, y + kLine / 2.0f) + oy, 1.0f, WithAlpha(kMenuTextColor, alpha));
        y += kLine;
    }
    // The button: amber, the cup and the words in the page's dark. Selected
    // (or under the pointer): a white ring, a lighter amber.
    const bool selected = m_focused && m_focus == Focus::Button && !m_mouseLeft;
    const bool lit = m_buttonHover || selected;
    // Its click: squished, then springing back a little past its size - and a ring spreading out.
    float scale = 1.0f;
    if (m_pressTime >= 0.0f)
    {
        constexpr float kPi = 3.14159265f;
        const float t = m_pressTime;
        scale = t < 0.12f ? 1.0f - 0.1f * std::sin(t / 0.12f * kPi / 2.0f)
              : t < 0.3f  ? 0.9f + 0.14f * std::sin((t - 0.12f) / 0.18f * kPi / 2.0f)
                          : 1.04f - 0.04f * std::min(1.0f, (t - 0.3f) / 0.15f);
        const float spread = std::min(1.0f, t / 0.45f);
        const float grow = 9.0f * spread;
        ui.DrawQuadRounded(m_buttonX + ox - grow, m_buttonY + oy - grow, m_buttonW + grow * 2.0f, m_buttonH + grow * 2.0f,
                           WithAlpha({1.0f, 0.79f, 0.34f, 0.45f * (1.0f - spread)}, alpha), m_buttonH / 2.0f + grow);
    }
    const float w = m_buttonW * scale, h = m_buttonH * scale;
    const float bx = m_buttonX + ox + (m_buttonW - w) / 2.0f, by = m_buttonY + oy + (m_buttonH - h) / 2.0f;
    if (selected)
        ui.DrawQuadRounded(bx - 2.0f, by - 2.0f, w + 4.0f, h + 4.0f, WithAlpha({1.0f, 1.0f, 1.0f, 0.9f}, alpha), h / 2.0f + 2.0f);
    XrColor4f amber = kMenuSelectionColor;
    if (lit)
        amber = {1.0f, 0.86f, 0.5f, 1.0f};
    if (m_pressTime >= 0.0f && m_pressTime < 0.2f)
        amber = {1.0f, 0.93f, 0.72f, 1.0f}; // (a flash while pressed)
    ui.DrawQuadRounded(bx, by, w, h, WithAlpha(amber, alpha), h / 2.0f);
    const XrColor4f dark = WithAlpha({0.12f, 0.08f, 0.04f, 1.0f}, alpha);
    DrawCoffeeCup(ui, bx + 9.0f * scale, by + 3.0f * scale, 10.0f * scale, dark);
    ui.DrawText(bold, "Gift me a coffee", bx + 25.0f * scale, TextY(ui, bold, by + h / 2.0f), scale, dark);
}

void SupportPage::DrawSide(UiRenderer &ui, float ox, float oy, float alpha)
{
    const UiFontHandle body = m_res->bodyFont, bold = m_res->bodyBoldFont, small = m_res->captionFont;
    constexpr float kLine = 10.5f;
    float y = m_gameY + 4.0f;
    ui.DrawText(bold, "Yuno's Adventure", kContentX + ox, TextY(ui, bold, y + kLine / 2.0f) + oy, 1.0f, WithAlpha(kMenuSelectionColor, alpha));
    y += kLine + 3.0f;
    for (const std::string &line : m_sideLines)
    {
        ui.DrawText(body, line, kContentX + ox, TextY(ui, body, y + kLine / 2.0f) + oy, 1.0f, WithAlpha(kMenuDimTextColor, alpha));
        y += kLine;
    }
    y += 4.0f;
    const int best = m_settings ? m_settings->yunoBest : 0;
    ui.DrawText(bold, "Best: " + std::to_string(best), kContentX + ox, TextY(ui, bold, y + kLine / 2.0f) + oy, 1.0f,
                WithAlpha(kMenuTextColor, alpha));
    y += kLine + 2.0f;
    const char *how = m_profile == ButtonMappingProfile::Desktop ? "Jump: Space, Enter, a click, or a gamepad's buttons"
                      : m_profile == ButtonMappingProfile::Phone ? "Jump: tap the game, or the button"
                                                                 : "Jump: A, or the trigger at the game";
    for (const std::string &line : Wrap(ui, small, how, m_gameX - kContentX - 10.0f))
    {
        ui.DrawText(small, line, kContentX + ox, TextY(ui, small, y + 4.0f) + oy, 1.0f, WithAlpha(kMenuDimTextColor, alpha));
        y += 8.5f;
    }
    if (m_jumpR > 0.0f)
    {
        const float cx = m_jumpX + ox, cy = m_jumpY + oy;
        ui.DrawQuadRounded(cx - m_jumpR, cy - m_jumpR, m_jumpR * 2.0f, m_jumpR * 2.0f, WithAlpha({0.5f, 0.03f, 0.03f, 1.0f}, alpha), m_jumpR);
        ui.DrawQuadRounded(cx - m_jumpR + 1.5f, cy - m_jumpR + 1.5f, m_jumpR * 2.0f - 3.0f, m_jumpR * 2.0f - 3.0f,
                           WithAlpha({0.85f, 0.08f, 0.08f, 1.0f}, alpha), m_jumpR - 1.5f);
        ui.DrawText(bold, "Jump", cx - ui.GetTextWidth(bold, "Jump") / 2.0f, TextY(ui, bold, cy), 1.0f, WithAlpha({1, 1, 1, 1}, alpha));
    }
}

void SupportPage::DrawSprite(UiRenderer &ui, int sprite, float gx, float gy, float ox, float oy, float alpha,
                             float brightness, int skipRows, float scale) const
{
    const Sprite &s = m_sprites[sprite];
    const float u = m_gameSize / kGame * scale;
    const int rows = s.h - skipRows;
    const float v0 = s.v0 + (s.v1 - s.v0) * skipRows / s.h;
    ui.DrawImageRegion(m_atlas, m_gameX + gx * (u / scale) + ox, m_gameY + gy * (u / scale) + oy, s.w * u, rows * u, s.u0, v0, s.u1, s.v1, alpha,
                       {brightness, brightness, brightness, 1.0f});
}

float SupportPage::Snap(float gamePixels) const
{
    // To the screen's own pixels (not the game's, which are about one and a
    // quarter of them): everything glides by in even steps, frame after frame.
    const float menuScale = m_res && m_res->appMenu ? m_res->appMenu->GetMenuScale() : kMenuScale;
    const float perGamePixel = m_gameSize / kGame * menuScale;
    return perGamePixel > 0.0f ? std::round(gamePixels * perGamePixel) / perGamePixel : gamePixels;
}

void SupportPage::DrawGame(UiRenderer &ui, float ox, float oy, float alpha)
{
    const float x = m_gameX + ox, y = m_gameY + oy, size = m_gameSize, u = size / kGame;
    const bool selected = m_focused && m_focus == Focus::Game;
    if (selected)
        ui.DrawQuadRounded(x - 2.0f, y - 2.0f, size + 4.0f, size + 4.0f, WithAlpha(kMenuSelectionColor, alpha), 6.0f);
    else
        ui.DrawQuadRounded(x - 1.0f, y - 1.0f, size + 2.0f, size + 2.0f, WithAlpha(kMenuLineColor, alpha), 5.0f);
    ui.DrawQuadRounded(x, y, size, size, WithAlpha({0.0f, 0.0f, 0.0f, 1.0f}, alpha), 4.0f);
    if (!m_atlas.IsValid())
        return;
    ui.SetClipRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f);

    // The sky, far away.
    for (const Cloud &c : m_clouds)
        DrawSprite(ui, c.sprite, Snap(c.x), c.y, ox, oy, alpha, 0.55f);
    // The mountains.
    for (const Mountain &m : m_mountains)
        DrawSprite(ui, m.sprite, m.x - Snap(m_mountainScroll), kHillY - m_sprites[m.sprite].h + 1.0f, ox, oy, alpha, 0.42f);
    // Nucita, far away at the mountains' feet, now and then during a run.
    if (m_state != State::Title)
    {
        if (m_nucita != NucitaMode::Away)
        {
            int sprite;
            float lift = 0.0f;
            if (m_nucitaHop > 0.0f)
            {
                sprite = m_sNucitaHop[m_nucitaHop > 0.25f ? 0 : 1];
                lift = std::sin((0.5f - m_nucitaHop) / 0.5f * 3.14159f) * 8.0f;
            }
            else if (m_nucita == NucitaMode::Walking)
                sprite = m_sNucitaWalk[static_cast<int>(m_nucitaTimer * 7.0f) % 4];
            else
                sprite = std::fmod(m_nucitaTimer, 2.4f) > 2.25f ? m_sNucitaBlink : m_sNucitaSit;
            DrawSprite(ui, sprite, Snap(m_nucitaX), kHillY - m_sprites[sprite].h - lift, ox, oy, alpha, 0.55f);
        }
    }

    // The ground.
    for (const Piece &p : m_pieces)
    {
        const float px = p.x - Snap(m_scroll);
        if (px > kGame || px + p.w < 0.0f)
            continue;
        switch (p.kind)
        {
        case PieceKind::Ground:
            DrawSprite(ui, m_sGround, px, kGroundY, ox, oy, alpha);
            DrawSprite(ui, m_sFill, px, kGroundY + m_sprites[m_sGround].h, ox, oy, alpha);
            break;
        case PieceKind::EdgeLeft:
            DrawSprite(ui, m_sEdgeL, px, kGroundY, ox, oy, alpha);
            DrawSprite(ui, m_sEdgeL, px, kGroundY + m_sprites[m_sEdgeL].h, ox, oy, alpha, 1.0f, 3);
            break;
        case PieceKind::EdgeRight:
            DrawSprite(ui, m_sEdgeR, px, kGroundY, ox, oy, alpha);
            DrawSprite(ui, m_sEdgeR, px, kGroundY + m_sprites[m_sEdgeR].h, ox, oy, alpha, 1.0f, 3);
            break;
        case PieceKind::Gap:
            break;
        }
    }
    for (const Obstacle &o : m_obstacles)
        DrawSprite(ui, o.sprite, o.x - Snap(m_scroll), kGroundY - m_sprites[o.sprite].h * kObstacleScale + 2.0f, ox, oy, alpha,
                   1.0f, 0, kObstacleScale);

    const UiFontHandle bold = m_res->bodyBoldFont, small = m_res->captionBoldFont;
    auto center = [&](UiFontHandle font, const std::string &text, float gy, const XrColor4f &color)
    {
        ui.DrawText(font, text, x + size / 2.0f - ui.GetTextWidth(font, text) / 2.0f, TextY(ui, font, y + gy * u), 1.0f,
                    WithAlpha(color, alpha));
    };
    const bool blinkOn = std::fmod(m_blinkTime, 1.0f) < 0.65f;
    const char *start = m_profile == ButtonMappingProfile::Phone ? "Tap to play"
                        : m_profile == ButtonMappingProfile::Desktop ? "Space to play"
                                                                    : "A to play";

    if (m_state == State::Title)
    {
        // Yuno, sitting in the middle.
        DrawSprite(ui, m_sSit, std::round((kGame - m_sprites[m_sSit].w) / 2.0f), kGroundY - m_sprites[m_sSit].h + 1.0f, ox, oy,
                   alpha);
        center(bold, "YUNO'S ADVENTURE", 22.0f, kRed);
        const int best = m_settings ? m_settings->yunoBest : 0;
        if (best > 0)
            center(small, "BEST " + Digits(best), 39.0f, kDimRed);
        if (selected ? blinkOn : true)
            center(small, start, 58.0f, selected ? kRed : kDimRed);
    }
    else
    {
        // Yuno.
        const bool air = !m_onGround || m_state == State::Falling;
        const int frame = air ? m_sJump : m_sRun[static_cast<int>(m_runPhase) % 6];
        const int shown = m_state == State::GameOver && m_yunoY > kGame ? -1 : frame;
        if (shown >= 0)
            DrawSprite(ui, shown, kYunoX, Snap(m_yunoY) - m_sprites[shown].h + 1.0f, ox, oy, alpha);
        // The score.
        const std::string hud = (m_settings && m_settings->yunoBest > 0 ? "HI " + Digits(m_settings->yunoBest) + "  " : "") +
                                Digits(Score());
        ui.DrawText(small, hud, x + size - 5.0f - ui.GetTextWidth(small, hud), TextY(ui, small, y + 12.0f * u), 1.0f,
                    WithAlpha(kRed, alpha));
        if (m_state == State::GameOver)
        {
            center(bold, "GAME OVER", 34.0f, kRed);
            center(small, m_newBest ? "NEW BEST! " + Digits(Score()) : "SCORE " + Digits(Score()), 50.0f, m_newBest ? kRed : kDimRed);
            if (m_overTime > 0.45f && blinkOn)
                center(small, m_profile == ButtonMappingProfile::Phone     ? "Tap to try again"
                       : m_profile == ButtonMappingProfile::Desktop ? "Space to try again"
                                                                    : "A to try again", 63.0f, kDimRed);
        }
    }
    ui.ResetClipRect();
}
