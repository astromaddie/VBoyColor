#include "achievements/Achievements.h"
#include "io/Platform.h"

#include "Version.h" // kGeneratedVersionString (the User-Agent)

#include <libretro.h> // the core's memory (retro_get_memory_data)
#include <md5.h>      // rcheevos' (src/rhash)
#include <rc_client.h>
#include <rc_consoles.h>

#include "stb_image.h" // (implemented in VulkanRenderer.cpp)

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace
{
    // The saved login, in the settings folder (see DataKindOf).
    constexpr const char *kAccountFile = "retroachievements.txt";

    void LogLine(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
#ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_INFO, "VBoyColor", fmt, args);
#else
        std::vfprintf(stderr, fmt, args);
        std::fputc('\n', stderr);
#endif
        va_end(args);
    }

    Achievements *Self(const rc_client_t *client) { return static_cast<Achievements *>(rc_client_get_userdata(client)); }

    std::string Str(const char *text) { return text ? text : ""; }

    // Where a badge's picture is (the server's address for it, or one made
    // from its name) - the unlocked one or the locked (gray) one.
    std::string BadgeUrl(const rc_client_achievement_t *achievement, bool unlocked)
    {
        char url[512] = {};
        const int state = unlocked ? RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED : RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
        return achievement && rc_client_achievement_get_image_url(achievement, state, url, sizeof(url)) == RC_OK ? url : "";
    }

    std::string GameBadgeUrl(const rc_client_game_t *game)
    {
        char url[512] = {};
        return game && rc_client_game_get_image_url(game, url, sizeof(url)) == RC_OK ? url : "";
    }
} // namespace

Achievements::~Achievements()
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (m_client)
        rc_client_destroy(m_client);
    m_client = nullptr;
}

void Achievements::Initialize(Platform &platform, UiRenderer &ui)
{
    m_platform = &platform;
    m_ui = &ui;

    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    m_client = rc_client_create(&Achievements::ReadMemory, &Achievements::ServerCall);
    if (!m_client)
        return;
    rc_client_set_userdata(m_client, this);
    rc_client_set_event_handler(m_client, &Achievements::OnEvent);
    rc_client_enable_logging(m_client, RC_CLIENT_LOG_LEVEL_WARN, &Achievements::Log);
    rc_client_set_hardcore_enabled(m_client, 0); // softcore only (for now)
    // Memory is read in DoFrame only - where it's the game's (not a library
    // thumbnail's) and the emulation thread holds the core.
    rc_client_set_allow_background_memory_reads(m_client, 0);
    // (development: a stand-in server, e.g. VBOYCOLOR_RA_HOST=http://127.0.0.1:8765)
    if (const char *host = std::getenv("VBOYCOLOR_RA_HOST"))
        rc_client_set_host(m_client, host);

    // "VBoyColor/1.0.0-beta.6 (windows) rcheevos/12.5.0" - RetroAchievements
    // asks clients to say who they are.
    char clause[64] = {};
    rc_client_get_user_agent_clause(m_client, clause, sizeof(clause));
    const std::string where = platform.UpdatePackage();
    std::string version = kGeneratedVersionString;
    if (!version.empty() && version[0] == 'v')
        version.erase(0, 1);
    m_userAgent = "VBoyColor/" + version + (where.empty() ? "" : " (" + where + ")") + " " + clause;

    m_atlasPixels.assign(static_cast<size_t>(kBadgeSize * kAtlasCells) * kBadgeSize * kAtlasCells * 4, 0);
    m_atlas = ui.CreateStreamingImage(kBadgeSize * kAtlasCells, kBadgeSize * kAtlasCells, VK_FORMAT_B8G8R8A8_SRGB);
    m_atlasDirty = true;
    m_badgeReady.assign(kAtlasCells * kAtlasCells, false);
}

// ---------------------------------------------------------------------------
// rc_client's callbacks

uint32_t Achievements::ReadMemory(uint32_t address, uint8_t *buffer, uint32_t numBytes, rc_client_t *)
{
    // rcheevos' Virtual Boy map: 0x000000-0x00FFFF the work RAM (0x05000000),
    // 0x010000-0x01FFFF the cartridge's save RAM (0x06000000). The core
    // allocates both anew for every game: asked for each time.
    const bool wram = address < 0x10000;
    if (address >= 0x20000)
        return 0;
    const unsigned id = wram ? RETRO_MEMORY_SYSTEM_RAM : RETRO_MEMORY_SAVE_RAM;
    const uint32_t offset = wram ? address : address - 0x10000;
    const auto *data = static_cast<const uint8_t *>(retro_get_memory_data(id));
    const size_t size = retro_get_memory_size(id);
    if (!data || offset >= size)
        return 0;
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(numBytes, size - offset));
    std::memcpy(buffer, data + offset, count);
    return count;
}

void Achievements::ServerCall(const rc_api_request_t *request, void (*callback)(const rc_api_server_response_t *, void *),
                              void *callbackData, rc_client_t *client)
{
    Achievements *self = Self(client);
    Pending pending;
    pending.callback = callback;
    pending.callbackData = callbackData;
    std::lock_guard<std::mutex> lock(self->m_mutex);
    self->m_queued.push_back({pending, {Str(request->url), Str(request->post_data), Str(request->content_type)}});
}

void Achievements::Log(const char *message, const rc_client_t *)
{
    LogLine("[RetroAchievements] %s", message);
}

void Achievements::OnEvent(const rc_client_event_t *event, rc_client_t *client)
{
    Achievements *self = Self(client);
    switch (event->type)
    {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    {
        const rc_client_achievement_t *a = event->achievement;
        Notice notice;
        notice.caption = "Achievement unlocked - " + std::to_string(a->points) + (a->points == 1 ? " point" : " points");
        notice.title = Str(a->title);
        notice.text = Str(a->description);
        notice.badge = self->Badge(BadgeUrl(a, true));
        self->AddNotice(std::move(notice));
        ++self->m_version;
        break;
    }
    case RC_CLIENT_EVENT_GAME_COMPLETED:
    {
        Notice notice;
        notice.caption = "Every achievement unlocked";
        const rc_client_game_t *game = rc_client_get_game_info(client);
        notice.title = game ? Str(game->title) : "";
        notice.text = "Congratulations!";
        notice.badge = game ? self->Badge(GameBadgeUrl(game)) : -1;
        notice.seconds = 7.0f;
        self->AddNotice(std::move(notice));
        break;
    }
    case RC_CLIENT_EVENT_SERVER_ERROR:
    {
        Notice notice;
        notice.caption = "RetroAchievements";
        notice.title = "The server said no";
        notice.text = event->server_error ? Str(event->server_error->error_message) : "";
        self->AddNotice(std::move(notice));
        break;
    }
    case RC_CLIENT_EVENT_DISCONNECTED:
        self->AddNotice({"RetroAchievements", "No connection", "Unlocks will be sent once it's back", -1, 5.0f});
        break;
    case RC_CLIENT_EVENT_RECONNECTED:
        self->AddNotice({"RetroAchievements", "Connected again", "Every unlock is sent", -1, 4.0f});
        break;
    default: // (leaderboards and challenges - hardcore's)
        break;
    }
}

void Achievements::LoginDone(int result, const char *errorMessage, rc_client_t *client, void *userdata)
{
    auto *self = static_cast<Achievements *>(userdata);
    const rc_client_user_t *user = rc_client_get_user_info(client);
    if (result == RC_OK && user)
    {
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_user = Str(user->username);
            self->m_token = Str(user->token);
            self->m_loginError.clear();
        }
        self->m_login = Login::LoggedIn;
        self->SaveAccount();
        LogLine("[RetroAchievements] Logged in as %s", user->username);
        // The game playing, if any, gets its achievements now.
        if (self->m_game.load() == Game::None && !self->m_hash.empty())
            self->StartLoadingGame();
    }
    else
    {
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_loginError = errorMessage && *errorMessage ? errorMessage : "Couldn't log in";
            if (result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN || result == RC_ACCESS_DENIED)
            {
                if (self->m_tokenLogin)
                    self->m_loginError = "The saved login no longer works - log in again";
                self->m_token.clear(); // (the saved login is no good: log in again)
            }
            else if (self->m_tokenLogin)
                self->m_loginError = "Couldn't reach RetroAchievements - " + self->m_loginError;
        }
        self->m_login = Login::LoggedOut;
        if (result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN || result == RC_ACCESS_DENIED)
            self->SaveAccount();
        LogLine("[RetroAchievements] Login failed (%d): %s", result, errorMessage ? errorMessage : "");
    }
    ++self->m_version;
}

void Achievements::LoadDone(int result, const char *errorMessage, rc_client_t *client, void *userdata)
{
    auto *self = static_cast<Achievements *>(userdata);
    const rc_client_game_t *game = rc_client_get_game_info(client);
    if (result == RC_OK && game)
    {
        rc_client_user_game_summary_t summary{};
        rc_client_get_user_game_summary(client, &summary);
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_gameTitle = Str(game->title);
            self->m_gameError.clear();
        }
        self->m_game = Game::Loaded;
        LogLine("[RetroAchievements] %s: %u of %u achievements unlocked", game->title, summary.num_unlocked_achievements,
                summary.num_core_achievements);
        if (summary.num_core_achievements > 0)
        {
            Notice notice;
            notice.caption = "RetroAchievements";
            notice.title = Str(game->title);
            notice.text = std::to_string(summary.num_unlocked_achievements) + " of " +
                          std::to_string(summary.num_core_achievements) + " achievements unlocked";
            notice.badge = self->Badge(GameBadgeUrl(game));
            notice.seconds = 4.0f;
            self->AddNotice(std::move(notice));
        }
    }
    else if (result == RC_NO_GAME_LOADED)
    {
        self->m_game = Game::Unknown;
        LogLine("[RetroAchievements] Unknown game (MD5 %s)", self->m_hash.c_str());
    }
    else if (result != RC_ABORTED)
    {
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_gameError = errorMessage && *errorMessage ? errorMessage : "Couldn't load the achievements";
        }
        self->m_game = result == RC_LOGIN_REQUIRED ? Game::None : Game::Failed;
        LogLine("[RetroAchievements] Loading the game failed (%d): %s", result, errorMessage ? errorMessage : "");
    }
    ++self->m_version;
}

// ---------------------------------------------------------------------------
// The account

void Achievements::LoadAccount()
{
    const std::vector<uint8_t> bytes = m_platform->ReadRomsFile(kAccountFile, true);
    std::string text(bytes.begin(), bytes.end()), user, token;
    bool notices = true;
    size_t at = 0;
    while (at < text.size())
    {
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t space = line.find(' ');
        if (space == std::string::npos)
            continue;
        const std::string key = line.substr(0, space), value = line.substr(space + 1);
        if (key == "user")
            user = value;
        else if (key == "token")
            token = value;
        else if (key == "notices")
            notices = value != "0";
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_user = user;
        m_token = token;
    }
    m_showNotices = notices;
    if (!user.empty() && !token.empty())
    {
        m_tokenLogin = true;
        m_login = Login::LoggingIn;
        std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
        rc_client_begin_login_with_token(m_client, user.c_str(), token.c_str(), &Achievements::LoginDone, this);
    }
}

void Achievements::SaveAccount()
{
    std::string text = "VBoyColor RetroAchievements 1\n";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_user.empty())
            text += "user " + m_user + "\n";
        if (!m_token.empty())
            text += "token " + m_token + "\n";
    }
    text += std::string("notices ") + (m_showNotices.load() ? "1" : "0") + "\n";
    m_platform->WriteRomsFile(kAccountFile, true, text.data(), text.size());
}

std::string Achievements::UserName() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_user;
}

uint32_t Achievements::Score() const
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    const rc_client_user_t *user = m_client ? rc_client_get_user_info(m_client) : nullptr;
    return user ? user->score_softcore : 0;
}

std::string Achievements::LoginError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loginError;
}

void Achievements::LogIn(const std::string &user, const std::string &password)
{
    if (!m_client || m_login.load() == Login::LoggingIn)
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loginError.clear();
    }
    m_login = Login::LoggingIn;
    m_tokenLogin = false;
    ++m_version;
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    rc_client_begin_login_with_password(m_client, user.c_str(), password.c_str(), &Achievements::LoginDone, this);
}

void Achievements::LogOut()
{
    {
        std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
        if (m_client)
        {
            rc_client_unload_game(m_client);
            rc_client_logout(m_client);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_token.clear(); // (the name stays, for the next login)
        m_loginError.clear();
    }
    m_login = Login::LoggedOut;
    m_game = Game::None;
    SaveAccount();
    ++m_version;
}

void Achievements::SetShowNotices(bool show)
{
    m_showNotices = show;
    SaveAccount();
}

// ---------------------------------------------------------------------------
// The game

void Achievements::GameLoaded(const std::vector<uint8_t> &rom, bool movie)
{
    {
        std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
        if (m_client)
            rc_client_unload_game(m_client);
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gameTitle.clear();
        m_gameError.clear();
    }
    m_hash.clear();
    m_game = movie && !rom.empty() ? Game::Movie : Game::None;
    ++m_version;
    if (rom.empty() || movie || !m_client)
        return;

    // A Virtual Boy game is known by the MD5 of its whole ROM file.
    md5_state_t md5;
    md5_init(&md5);
    md5_append(&md5, rom.data(), static_cast<int>(rom.size()));
    md5_byte_t digest[16];
    md5_finish(&md5, digest);
    char hex[33] = {};
    for (int i = 0; i < 16; ++i)
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    m_hash = hex;
    if (m_login.load() != Login::LoggedOut)
        StartLoadingGame();
}

void Achievements::RetryGame()
{
    const Game game = m_game.load();
    if (!m_hash.empty() && m_login.load() == Login::LoggedIn && (game == Game::Failed || game == Game::None))
        StartLoadingGame();
}

void Achievements::StartLoadingGame()
{
    m_game = Game::Loading;
    ++m_version;
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    rc_client_begin_load_game(m_client, m_hash.c_str(), &Achievements::LoadDone, this);
}

void Achievements::DoFrame()
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (m_client)
        rc_client_do_frame(m_client);
}

void Achievements::GameReset()
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (m_client)
        rc_client_reset(m_client);
}

std::vector<uint8_t> Achievements::SaveProgress()
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (!m_client || !rc_client_is_game_loaded(m_client))
        return {};
    std::vector<uint8_t> progress(rc_client_progress_size(m_client));
    if (progress.empty() || rc_client_serialize_progress_sized(m_client, progress.data(), progress.size()) != RC_OK)
        return {};
    return progress;
}

void Achievements::LoadProgress(const std::vector<uint8_t> &progress)
{
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (!m_client || !rc_client_is_game_loaded(m_client))
        return;
    if (progress.empty())
        rc_client_deserialize_progress_sized(m_client, nullptr, 0); // (starts over)
    else
        rc_client_deserialize_progress_sized(m_client, progress.data(), progress.size());
}

std::string Achievements::GameTitle() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gameTitle;
}

std::string Achievements::GameError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gameError;
}

std::vector<Achievements::Item> Achievements::Items(uint32_t &unlocked, uint32_t &total, uint32_t &points,
                                                     uint32_t &totalPoints)
{
    std::vector<Item> items;
    unlocked = total = points = totalPoints = 0;
    std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
    if (!m_client || !rc_client_is_game_loaded(m_client) || m_game.load() != Game::Loaded)
        return items;
    rc_client_user_game_summary_t summary{};
    rc_client_get_user_game_summary(m_client, &summary);
    unlocked = summary.num_unlocked_achievements;
    total = summary.num_core_achievements;
    points = summary.points_unlocked;
    totalPoints = summary.points_core;

    rc_client_achievement_list_t *list = rc_client_create_achievement_list(
        m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list)
        return items;
    for (uint32_t b = 0; b < list->num_buckets; ++b)
    {
        const rc_client_achievement_bucket_t &bucket = list->buckets[b];
        for (uint32_t i = 0; i < bucket.num_achievements; ++i)
        {
            const rc_client_achievement_t *a = bucket.achievements[i];
            Item item;
            item.group = Str(bucket.label);
            item.title = Str(a->title);
            item.description = Str(a->description);
            item.progress = a->measured_progress;
            item.id = a->id;
            item.points = a->points;
            item.unlocked = (a->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) != 0;
            item.badge = Badge(BadgeUrl(a, item.unlocked));
            items.push_back(std::move(item));
        }
    }
    rc_client_destroy_achievement_list(list);
    return items;
}

// ---------------------------------------------------------------------------
// Notices and badges

void Achievements::AddNotice(Notice notice)
{
    if (!m_showNotices)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_notices.size() < 16)
        m_notices.push_back(std::move(notice));
}

bool Achievements::TakeNotice(Notice &notice)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_notices.empty())
        return false;
    notice = std::move(m_notices.front());
    m_notices.pop_front();
    return true;
}

int Achievements::Badge(const std::string &url)
{
    if (url.empty())
        return -1;
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_badgeOf.find(url);
    if (found != m_badgeOf.end())
        return found->second;
    // A cell of the atlas - the oldest picture makes room when it's full.
    const int badge = m_nextBadge;
    m_nextBadge = (m_nextBadge + 1) % (kAtlasCells * kAtlasCells);
    for (auto it = m_badgeOf.begin(); it != m_badgeOf.end(); ++it)
        if (it->second == badge)
        {
            m_badgeOf.erase(it);
            break;
        }
    m_badgeOf[url] = badge;
    if (badge < static_cast<int>(m_badgeReady.size()))
        m_badgeReady[badge] = false;
    Pending pending;
    pending.kind = 1;
    pending.badge = badge;
    m_queued.push_back({pending, {url, "", ""}});
    return badge;
}

void Achievements::BadgeArrived(int badge, const std::vector<uint8_t> &png)
{
    int w = 0, h = 0, channels = 0;
    stbi_uc *rgba = png.empty() ? nullptr
                                : stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &channels, 4);
    if (!rgba || w <= 0 || h <= 0)
    {
        if (rgba)
            stbi_image_free(rgba);
        LogLine("[RetroAchievements] A badge's picture didn't decode (%zu bytes)", png.size());
        return;
    }
    // Into its 64x64 cell (RetroAchievements' badges are that size; others scaled, nearest).
    const int stride = kBadgeSize * kAtlasCells;
    const int cx = (badge % kAtlasCells) * kBadgeSize, cy = (badge / kAtlasCells) * kBadgeSize;
    for (int y = 0; y < kBadgeSize; ++y)
        for (int x = 0; x < kBadgeSize; ++x)
        {
            const stbi_uc *p = rgba + (static_cast<size_t>(y * h / kBadgeSize) * w + x * w / kBadgeSize) * 4;
            uint8_t *q = &m_atlasPixels[(static_cast<size_t>(cy + y) * stride + cx + x) * 4];
            q[0] = p[2];
            q[1] = p[1];
            q[2] = p[0];
            q[3] = p[3];
        }
    stbi_image_free(rgba);
    m_atlasDirty = true;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (badge < static_cast<int>(m_badgeReady.size()))
        m_badgeReady[badge] = true;
}

bool Achievements::BadgeUv(int badge, float &u0, float &v0, float &u1, float &v1) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (badge < 0 || badge >= static_cast<int>(m_badgeReady.size()) || !m_badgeReady[badge])
        return false;
    const float cell = 1.0f / kAtlasCells;
    u0 = (badge % kAtlasCells) * cell;
    v0 = (badge / kAtlasCells) * cell;
    u1 = u0 + cell;
    v1 = v0 + cell;
    return true;
}

// ---------------------------------------------------------------------------
// Each app frame

void Achievements::Update(float /*deltaSeconds*/, bool gameRunning)
{
    if (!m_client || !m_platform)
        return;
    if (!m_accountLoaded && m_platform->HasRomsFolder())
    {
        m_accountLoaded = true;
        LoadAccount();
    }

    // Start what's queued.
    std::deque<std::pair<Pending, std::vector<std::string>>> queued;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        queued.swap(m_queued);
    }
    for (auto &[pending, call] : queued)
    {
        Platform::HttpRequest request;
        request.url = call[0];
        request.postData = call[1];
        request.contentType = call[2];
        request.userAgent = m_userAgent;
        const int id = m_platform->StartHttpRequest(request);
        if (id > 0)
        {
            m_running[id] = pending;
            continue;
        }
        if (pending.kind == 0)
        {
            // This platform can't (or wouldn't): rc_client hears it failed.
            rc_api_server_response_t response{};
            response.http_status_code = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
            std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
            pending.callback(&response, pending.callbackData);
        }
    }

    // Hand back what's finished.
    for (auto it = m_running.begin(); it != m_running.end();)
    {
        int status = -1;
        std::vector<uint8_t> body;
        const int state = m_platform->PollHttpRequest(it->first, status, body);
        if (state == 0)
        {
            ++it;
            continue;
        }
        const Pending pending = it->second;
        it = m_running.erase(it);
        if (pending.kind == 1)
        {
            if (state == 1 && status == 200)
                BadgeArrived(pending.badge, body);
            else
                LogLine("[RetroAchievements] A badge didn't load (%d, HTTP %d, %zu bytes)", state, status, body.size());
            continue;
        }
        rc_api_server_response_t response{};
        body.push_back(0); // (rcheevos reads the body as text)
        response.body = reinterpret_cast<const char *>(body.data());
        response.body_length = body.size() - 1;
        // No connection: rc_client tries again later (an unlock stays queued).
        response.http_status_code = state == 1 && status > 0 ? status : RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
        std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
        pending.callback(&response, pending.callbackData);
    }

    // The session's pings and retries while frames aren't running.
    if (!gameRunning)
    {
        std::lock_guard<std::recursive_mutex> rc(m_rcMutex);
        rc_client_idle(m_client);
    }

    if (m_atlasDirty && m_ui)
    {
        m_atlasDirty = false;
        m_ui->UpdateStreamingImage(m_atlas, m_atlasPixels.data(), m_atlasPixels.size());
    }
}
