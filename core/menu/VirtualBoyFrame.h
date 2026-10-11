#pragma once

#include "gfx/UiRenderer.h"

#include <cstdint>
#include <future>
#include <vector>

// The phone app's menu with no game loaded, seen as if looking into a
// Virtual Boy: the menu deep inside, a black rubber eyeshade round it (the
// nose's notch at its bottom), the red body round that - one object, lit
// from above. The menu stays as big as it fits: the frame shows in the room
// beside it (a phone turned sideways: left and right - above and below, it
// runs off the screen).
//
// Its picture is made on the CPU for the screen's size (on a thread of its
// own, at most ~1280 pixels wide - drawn scaled up), once per size.
class VirtualBoyFrame
{
public:
    // Where everything goes on a w x h screen (pixels).
    struct Geometry
    {
        float menuScale = 1.0f;    // the menu's scale (kMenuWidth x kMenuHeight units)
        float menuX = 0, menuY = 0; // its top left corner
        float cx = 0, cy = 0;       // the middle of it all
        float cavityHW = 0, cavityHH = 0, cavityR = 0; // the dark inside (half sizes, corner radius)
        float rubberHW = 0, rubberHH = 0, rubberR = 0, rubber = 0; // the eyeshade (rubber: its width)
        float noseY = 0, noseR = 0; // its notch (a circle's center and radius)
        float bodyHW = 0, bodyHH = 0, bodyR = 0, body = 0; // the red body (body: its width round the eyeshade)
        float shadow = 0;           // how far its shadow reaches
        bool sides = true;          // the frame's left and right of the menu (else above and below)
    };
    static Geometry Layout(float w, float h);

    // The picture, BGRA (sRGB) texW x texH, for a w x h screen.
    static std::vector<uint8_t> Make(const Geometry &g, float w, float h, int texW, int texH);

    // Once a frame, outside rendering: makes the picture for a w x h screen
    // when it's needed and isn't made yet (then uploads it).
    void Update(UiRenderer &ui, float w, float h);
    // The whole screen (alpha: 0-1); nothing until the picture's made (then
    // it fades in).
    void Draw(UiRenderer &ui, float w, float h, float alpha);

private:
    struct Made
    {
        int w = 0, h = 0;       // for this screen
        int texW = 0, texH = 0;
        std::vector<uint8_t> bgra;
        UiImageHandle image;
    };
    std::vector<Made> m_made; // (a texture per screen size - a phone has two, turned or not)
    std::future<Made> m_making;
    int m_makingW = 0, m_makingH = 0;
    int m_shown = -1;         // which m_made shows
    float m_fadeIn = 0.0f;
};
