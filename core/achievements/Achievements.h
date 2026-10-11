#pragma once

#include "gfx/UiRenderer.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class Platform;
struct rc_client_t;
struct rc_api_request_t;
struct rc_api_server_response_t;
struct rc_client_event_t;

// RetroAchievements (retroachievements.org) through rcheevos' rc_client, in
// softcore: save states, rewinding a state and the rest stay allowed, and an
// unlock counts as a softcore one.
//
// The player logs in once with a user name and password (Achievements page);
// only the token the server hands back is kept (retroachievements.txt, in
// the settings folder), and the next start logs in with it. A game is known
// to RetroAchievements by the MD5 of its ROM; its achievements load when it
// does, and each emulated frame is checked against them on the emulation
// thread (DoFrame). Unlocks, the game's start and connection trouble become
// notices (TakeNotice) the menu shows over the game.
//
// Threads: DoFrame runs on the emulation thread (the core's memory valid -
// m_emuMutex held); everything else on the render thread. Every rc_client
// call is made holding m_rcMutex; web requests are only queued from inside
// rc_client (ServerCall) and started and finished by Update, which hands the
// replies back to rc_client.
class Achievements
{
public:
    Achievements() = default;
    ~Achievements();
    Achievements(const Achievements &) = delete;
    Achievements &operator=(const Achievements &) = delete;

    // At start: makes the client (softcore). The saved login is read by the
    // first Update that can reach the settings folder.
    void Initialize(Platform &platform, UiRenderer &ui);
    // Once per app frame, on the render thread, outside rendering (it may
    // upload badges): starts queued web requests and hands finished ones
    // back, keeps the session alive while the game doesn't run.
    void Update(float deltaSeconds, bool gameRunning);

    // ---- from the Emulator ----

    // A game loaded (or failed to: rom empty). A movie (a tool-assisted run)
    // plays without achievements.
    void GameLoaded(const std::vector<uint8_t> &rom, bool movie);
    // After every emulated frame - the emulation thread.
    void DoFrame();
    // The game reset (power-on).
    void GameReset();
    // A save state carries the achievements' progress (in a file beside it):
    // what to keep - empty when there's nothing.
    std::vector<uint8_t> SaveProgress();
    // A save state loaded: its progress (empty: none was kept - progress
    // starts over, so nothing unlocks from memory that jumped).
    void LoadProgress(const std::vector<uint8_t> &progress);

    // ---- the account ----

    enum class Login
    {
        LoggedOut,
        LoggingIn,
        LoggedIn
    };
    Login GetLogin() const { return m_login.load(); }
    std::string UserName() const;
    uint32_t Score() const; // softcore points
    // Why the last login failed ("" if it didn't).
    std::string LoginError() const;
    void LogIn(const std::string &user, const std::string &password);
    void LogOut();
    // Unlocks (and the game's start) shown over the game.
    bool ShowNotices() const { return m_showNotices; }
    void SetShowNotices(bool show);

    // ---- the game ----

    enum class Game
    {
        None,      // no game, or nobody logged in
        Loading,   // asking RetroAchievements
        Loaded,    // its achievements are on
        Unknown,   // RetroAchievements doesn't know this ROM (another version of the game, say)
        Failed,    // couldn't ask (no connection...) - GameError says why
        Movie      // a tool-assisted run plays: no achievements
    };
    Game GetGame() const { return m_game.load(); }
    // Asks again for the game's achievements (after Failed - no connection, say).
    void RetryGame();
    std::string GameTitle() const;
    std::string GameError() const;
    // Changes whenever the game's achievements (or the login) do - a page
    // showing them rebuilds then.
    uint64_t Version() const { return m_version.load(); }
    struct Item
    {
        std::string group; // "Locked", "Unlocked", "Recently Unlocked", ...
        std::string title, description;
        std::string progress; // "3/10" when the achievement counts something
        uint32_t id = 0, points = 0;
        bool unlocked = false;
        int badge = -1; // see BadgeUv
    };
    std::vector<Item> Items(uint32_t &unlocked, uint32_t &total, uint32_t &points, uint32_t &totalPoints);

    // ---- badges: 64x64 pictures, in one atlas texture ----

    UiImageHandle BadgeAtlas() const { return m_atlas; }
    // A badge's place in BadgeAtlas - false until its picture has arrived.
    bool BadgeUv(int badge, float &u0, float &v0, float &u1, float &v1) const;

    // ---- notices for over the game ----

    struct Notice
    {
        std::string caption; // "Achievement unlocked"
        std::string title;
        std::string text;
        int badge = -1;
        float seconds = 5.0f;
    };
    bool TakeNotice(Notice &notice);

private:
    struct Pending
    {
        int kind = 0; // 0 a call of rc_client's, 1 a badge
        void (*callback)(const rc_api_server_response_t *, void *) = nullptr;
        void *callbackData = nullptr;
        int badge = -1;
    };

    // rc_client's callbacks.
    static uint32_t ReadMemory(uint32_t address, uint8_t *buffer, uint32_t numBytes, rc_client_t *client);
    static void ServerCall(const rc_api_request_t *request, void (*callback)(const rc_api_server_response_t *, void *),
                           void *callbackData, rc_client_t *client);
    static void OnEvent(const rc_client_event_t *event, rc_client_t *client);
    static void LoginDone(int result, const char *errorMessage, rc_client_t *client, void *userdata);
    static void LoadDone(int result, const char *errorMessage, rc_client_t *client, void *userdata);
    static void Log(const char *message, const rc_client_t *client);

    void LoadAccount();
    void SaveAccount();
    void StartLoadingGame();
    void AddNotice(Notice notice);
    // The badge picture at url: its index (the picture loads in the background).
    int Badge(const std::string &url);
    void BadgeArrived(int badge, const std::vector<uint8_t> &png);

    Platform *m_platform = nullptr;
    UiRenderer *m_ui = nullptr;
    rc_client_t *m_client = nullptr;
    mutable std::recursive_mutex m_rcMutex; // every rc_client call
    mutable std::mutex m_mutex;             // everything below shared with rc_client's callbacks
    std::string m_userAgent;

    bool m_accountLoaded = false;
    std::string m_user, m_token; // the saved login
    std::atomic<bool> m_showNotices{true};
    std::atomic<Login> m_login{Login::LoggedOut};
    std::string m_loginError;
    bool m_tokenLogin = false; // the login going on is the saved one (not typed)

    std::atomic<Game> m_game{Game::None};
    std::string m_hash; // the loaded ROM's MD5 ("" none)
    std::string m_gameTitle, m_gameError;
    std::atomic<uint64_t> m_version{1};

    // Web requests: queued (from rc_client), then running (by Platform id).
    std::deque<std::pair<Pending, std::vector<std::string>>> m_queued; // (url, postData, contentType)
    std::map<int, Pending> m_running;

    // Badges.
    static constexpr int kBadgeSize = 64, kAtlasCells = 16; // 16x16 cells of 64px: 1024x1024
    UiImageHandle m_atlas;
    std::vector<uint8_t> m_atlasPixels; // BGRA
    bool m_atlasDirty = false;
    std::map<std::string, int> m_badgeOf; // url -> badge
    std::vector<bool> m_badgeReady;
    int m_nextBadge = 0;

    std::deque<Notice> m_notices;
};
