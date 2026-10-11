#pragma once

#include "gfx/UiRenderer.h"

#include <cstdint>
#include <string>
#include <vector>

class Platform;

// The Virtual Boy controller on a touch screen (the phone app): laid out
// like the real one - the left D-pad with Select and Start below it, the
// right D-pad with B and A below it, L and R on top - with the game screen
// between the two sides, as big as it can be (the controls may reach a
// little over its edges). The right D-pad shows only in the games that use
// it (UsesRightDpad). A small button opens the menu (landscape: bottom left,
// off the picture).
//
// The player can move every control and make it bigger or smaller (edit
// mode - BeginEdit): kept per orientation in the settings folder
// (touch-layout.txt - Load/Save).
//
// Touches are read fresh every frame (Update), so a finger can slide from
// one button to another, and several fingers press several buttons.
// Hidden while a gamepad is in use (the phone app shows them again on the
// next touch).
class TouchControls
{
public:
    struct Touch
    {
        int id = 0;
        float x = 0, y = 0; // screen pixels
    };

    // Loads the font the labels are drawn with.
    void Initialize(UiRenderer &ui, Platform &platform);

    // Lays the controls out for a w x h screen (pixels, landscape or not),
    // pxPerDp: pixels per density-independent pixel (Android's dp - about
    // 1/160 inch); rightDpad: the game uses the right D-pad.
    void Layout(float w, float h, float pxPerDp, bool rightDpad);
    // Where the game screen goes: the space the controls leave (all of it
    // when they're hidden).
    void ScreenArea(float &x, float &y, float &w, float &h) const;

    // The Virtual Boy buttons the touches hold (bits of 1 << VBButtonBit::*),
    // and whether a touch just landed on the menu button.
    uint32_t Update(const std::vector<Touch> &touches, bool &menuPressed);
    // Buttons pressed this frame that weren't the last (for a tap of
    // haptic feedback).
    uint32_t NewlyPressed() const { return m_pressed & ~m_lastPressed; }

    void Draw(UiRenderer &ui) const;

    void SetVisible(bool visible) { m_visible = visible; }
    bool Visible() const { return m_visible; }

    // The games that use the right D-pad (by name): Red Alarm, Teleroboxer,
    // 3-D Tetris, Golf (and T&E Virtual Golf).
    static bool UsesRightDpad(const std::string &romName);

    // The player's own layout (positions and sizes; the defaults until then):
    // the general one (touch-layout.txt), or a game's own (<game>.touch,
    // made from the library - GameControlsPage). Save writes the one shown.
    void Load(Platform &platform);
    void Save(Platform &platform) const;
    // The game playing ("" none): its own layout if it has one, else the
    // general one - read when it changes. (Not while editing.)
    void SetGame(Platform &platform, const std::string &game);
    // Whose layout is shown: a game with its own, or "" (the general one).
    const std::string &LayoutGame() const { return m_layoutGame; }
    // A game has a layout of its own / give it back the general one.
    static bool HasGameLayout(Platform &platform, const std::string &game);
    void ForgetGameLayout(Platform &platform, const std::string &game);

    // Edit mode: every control shows; dragging one moves it, the bar at the
    // top makes the chosen one smaller or bigger, resets all, or finishes.
    // game: edit that game's own layout (it starts from what it has - its
    // own or the general one); "" the general one.
    void BeginEdit(Platform &platform, const std::string &game = "");
    bool Editing() const { return m_editing; }
    // Once a frame while editing: true when the player is done (the caller
    // saves - Save - and leaves edit mode - EndEdit).
    bool UpdateEdit(const std::vector<Touch> &touches);
    void EndEdit() { m_editing = false; }
    void DrawEdit(UiRenderer &ui) const;

    // "Back to game" while the menu shows over a game: right of the menu
    // (menuRight, menuTop: the menu's top right corner, screen pixels), or
    // over its top right corner when the screen has no room beside it.
    void LayoutResume(float menuRight, float menuTop);
    // True once when a touch lands on it.
    bool UpdateResume(const std::vector<Touch> &touches);
    void DrawResume(UiRenderer &ui) const;

private:
    struct Circle
    {
        float x = 0, y = 0, r = 0;
    };
    struct Rect
    {
        float x = 0, y = 0, w = 0, h = 0;
    };
    // Which direction bits a touch at (x, y) presses on a D-pad, 0 if it's
    // not on it.
    uint32_t DpadBits(const Circle &pad, float x, float y, uint32_t up, uint32_t down, uint32_t left, uint32_t right) const;
    void DrawDpad(UiRenderer &ui, const Circle &pad, uint32_t up, uint32_t down, uint32_t left, uint32_t right) const;
    // (the bar's and "Back to game"'s buttons - the controls are icons, DrawIcon)
    void DrawPill(UiRenderer &ui, const Rect &r, const char *label, bool pressed, UiFontHandle font) const;
    // One of the controls' icons (TouchIcon in the .cpp - Kenney's Input
    // Prompts), size pixels square around (cx, cy); pressed: red.
    void DrawIcon(UiRenderer &ui, int icon, float cx, float cy, float size, bool pressed) const;
    // The icon atlas with cells big enough for the controls as laid out
    // (64, 128 or 256 pixels - loaded when first needed).
    void ChooseAtlas();

    // The controls the player can move (Adjust: how they moved them).
    enum Element
    {
        kLeftPad,
        kRightPad,
        kA,
        kB,
        kL,
        kR,
        kSelect,
        kStart,
        kMenu,
        kElementCount
    };
    struct Adjust
    {
        float dx = 0.0f, dy = 0.0f; // the center's move, in screen widths / heights
        float scale = 1.0f;
    };
    // Applies the player's layout to the default one Layout made.
    void ApplyAdjust();
    // Which control a point is on (-1: none).
    int ElementAt(float x, float y) const;
    // An element's center and its box (for the edit outline).
    void ElementBox(int element, float &x, float &y, float &w, float &h) const;
    void MoveElement(int element, float dx, float dy);
    Adjust &AdjustOf(int element) { return m_adjust[m_h > m_w ? 1 : 0][element]; }

    UiRenderer *m_ui = nullptr;
    Platform *m_platform = nullptr;
    UiImageHandle m_atlases[3];
    int m_atlas = -1; // which m_atlases draws
    std::vector<uint8_t> m_fontBytes;
    float m_fontDp = 0.0f; // the density the fonts are baked for
    UiFontHandle m_font, m_smallFont;
    bool m_visible = true;
    bool m_rightDpad = false;
    float m_dp = 1.0f;
    float m_w = 0, m_h = 0;
    float m_screenX = 0, m_screenY = 0, m_screenW = 0, m_screenH = 0;
    Circle m_leftPad, m_rightPad, m_a, m_b, m_menu;
    Rect m_l, m_r, m_select, m_start;
    uint32_t m_pressed = 0, m_lastPressed = 0;
    bool m_menuDown = false;

    Adjust m_adjust[2][kElementCount]; // [landscape, portrait]
    bool Parse(const std::vector<uint8_t> &bytes); // a layout file into m_adjust (false: none in it)
    std::string m_forGame;    // the game SetGame was last given
    std::string m_layoutGame; // whose layout m_adjust holds ("" the general one)
    bool m_editing = false;
    bool m_editFresh = false; // (BeginEdit's frame)
    int m_selected = -1;
    int m_dragId = -1;             // the finger moving m_selected
    float m_dragX = 0, m_dragY = 0; // where it was last frame
    std::vector<int> m_lastIds;    // the fingers down last frame
    Rect m_editSmaller, m_editBigger, m_editReset, m_editDone;
    Rect m_resume;
    bool m_resumeDown = false;
};
