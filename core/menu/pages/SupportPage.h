#pragma once
#include "menu/MenuPage.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct AppSettings;
class Platform;
class Emulator;

// Support (the sidebar, above Quit): a few words from Juan and a "Buy me a
// coffee" button (his GitHub Sponsors page, one time) - and under it Yuno's
// Adventure, a little endless runner in the Virtual Boy's reds starring his
// cat Yuno (his sister Nucita shows up on the far hills now and then). Jump
// over rocks, bushes and gaps: A (any face button on a gamepad, Space or Enter on a
// keyboard), a click or a tap on the game, or the trigger pointing at it.
// The arrows still move out of it - and leaving the game (or the page, or
// the menu) ends the run. The best score is kept (AppSettings::yunoBest).
class SupportPage : public MenuPage
{
public:
    static constexpr const char *kSponsorUrl =
        "https://github.com/sponsors/yuk27/sponsorships?sponsor=yuk27&frequency=one-time&amount=1";

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    std::string Title() const override { return "Support"; }
    std::string Subtitle() const override { return "and Yuno's Adventure"; }
    std::vector<MenuHint> Hints() const override;
    bool HasFocusable() const override { return true; }
    void SetFocused(bool focused) override;
    void ResetSelection() override;
    void OnShow() override;
    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds) override;
    void Draw(UiRenderer &ui, int transitionDirX, int transitionDirY, float moveProgress, float moveDist,
              float fadeProgress) override;
    void HandlePointer(const MenuPointer &pointer) override;

    // A key pressed on a real keyboard (GLFW's code - see AppMenu::SubmitRawKey): Space jumps.
    void SubmitKey(int key);

    // The sidebar's icon (and the button's): a cup of coffee on its saucer, size x size.
    static void DrawCoffeeCup(UiRenderer &ui, float x, float y, float size, const XrColor4f &color);

private:
    enum class Focus
    {
        Button,
        Game
    };
    enum class State
    {
        Title,
        Running,
        Falling, // (into a gap - then GameOver)
        GameOver
    };
    // The ground, left to right: plain ground, a cliff's edges and the gap between them.
    enum class PieceKind
    {
        Ground,
        EdgeLeft, // (the ground ends - a gap follows)
        Gap,
        EdgeRight // (... and starts again)
    };
    struct Piece
    {
        float x, w; // world x, game pixels
        PieceKind kind;
    };
    struct Obstacle
    {
        float x; // world x
        int sprite;
    };
    struct Cloud
    {
        float x, y; // on screen (they drift on their own)
        int sprite;
    };
    struct Sprite
    {
        int w = 0, h = 0;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    };

    void BuildAtlas(UiRenderer &ui);
    void BuildSounds();
    int SpriteIndex(const char *name) const;

    void Activate();      // A / Space / a click on the game
    void StopGame();      // back to the title
    void StartRun();
    void Jump();
    void StepGame(float dt);
    void Generate();      // the ground and what's on it, ahead of the screen
    void EndRun(bool fell);
    int Score() const { return static_cast<int>(m_distance / 10.0f); }
    void PlaySound(const std::vector<int16_t> &sound);

    // Layout (menu units).
    void Layout(UiRenderer &ui);
    void DrawCard(UiRenderer &ui, float ox, float oy, float alpha);
    void DrawGame(UiRenderer &ui, float ox, float oy, float alpha);
    void DrawSide(UiRenderer &ui, float ox, float oy, float alpha);
    void DrawSprite(UiRenderer &ui, int sprite, float gx, float gy, float ox, float oy, float alpha,
                    float brightness = 1.0f, int skipRows = 0) const;

    const UiMenuResources *m_res = nullptr;
    AppSettings *m_settings = nullptr;
    Platform *m_platform = nullptr;
    Emulator *m_emulator = nullptr;
    ButtonMappingProfile m_profile = ButtonMappingProfile::Vr;

    bool m_focused = false;
    Focus m_focus = Focus::Button;
    bool m_buttonHover = false;

    // The card's text, wrapped (Layout), and where things are.
    bool m_laidOut = false;
    std::vector<std::string> m_cardLines, m_sideLines;
    float m_cardH = 0, m_buttonX = 0, m_buttonY = 0, m_buttonW = 0, m_buttonH = 0;
    float m_gameX = 0, m_gameY = 0, m_gameSize = 0; // the square
    float m_jumpX = 0, m_jumpY = 0, m_jumpR = 0;    // the phone's Jump button (radius 0: none)

    // The pictures (YunoSprites.h), in one texture.
    UiImageHandle m_atlas;
    std::vector<Sprite> m_sprites;
    int m_sRun[6]{}, m_sJump = 0, m_sSit = 0, m_sGround = 0, m_sFill = 0, m_sEdgeL = 0, m_sEdgeR = 0;
    int m_sNucitaSit = 0, m_sNucitaBlink = 0, m_sNucitaWalk[4]{}, m_sNucitaHop[2]{};
    std::vector<int> m_obstacleSprites, m_cloudSprites;

    std::vector<int16_t> m_jumpSound, m_overSound;

    // The run.
    State m_state = State::Title;
    std::mt19937 m_rng{12345};
    float m_scroll = 0;   // world x at the screen's left
    float m_speed = 0;    // game pixels a second
    float m_distance = 0; // (the score: a tenth of it)
    float m_runTime = 0;
    float m_yunoY = 0, m_yunoVy = 0; // his feet, game pixels from the top
    bool m_onGround = true;
    float m_jumpBuffer = 0; // (a jump pressed just before landing still counts)
    float m_animTime = 0;
    float m_overTime = 0;   // since the run ended
    bool m_newBest = false;
    std::vector<Piece> m_pieces;
    std::vector<Obstacle> m_obstacles;
    std::vector<Cloud> m_clouds;
    float m_genX = 0, m_nextEventX = 0, m_nextCloud = 0;
    // Nucita, far away on the hills.
    enum class NucitaMode
    {
        Away,
        Walking,
        Sitting
    } m_nucita = NucitaMode::Away;
    float m_nucitaX = 0, m_nucitaTimer = 0, m_nucitaNext = 0, m_nucitaHop = 0;
    int m_lastHundred = 0;
    float m_titleTime = 0;
};
