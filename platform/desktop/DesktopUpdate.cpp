// DesktopPlatform's updates: a release package (the CI's
// VBoyColor-windows-<version>.zip or VBoyColor-linux-<version>.tar.gz, one
// folder with the app inside) unpacked over the app's own folder.
//
// - The games folder that ships in the package (roms/, just a note) is left
//   alone - only its note is added if it's missing.
// - Every file is written beside its target first (<name>.new) and then
//   moved over it. Windows can't replace a running exe, but it can rename
//   one: the running exe (and anything else in use) moves aside as
//   <name>.old, removed at the next start (see the constructor).
// - Then the app asks to restart (RequestRestart): the main loop ends, saves
//   and shuts down as usual, and calls Relaunch.
//
// Deflate (zip entries, gzip) is stb_image's own zlib decoder - the app
// already carries it for PNGs.
#include "desktop/DesktopPlatform.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stb_image.h> // stbi_zlib_decode_noheader_malloc (implementation in VulkanRenderer.cpp)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace
{
    struct PackageFile
    {
        std::string path; // inside the package, '/'-separated, its top folder taken off
        std::vector<uint8_t> data;
        unsigned mode = 0644;
    };

    uint32_t Le16(const uint8_t *p) { return p[0] | (p[1] << 8); }
    uint32_t Le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

    // Raw deflate -> bytes ("" on a broken stream).
    bool Inflate(const uint8_t *data, size_t size, std::vector<uint8_t> &out)
    {
        int length = 0;
        char *bytes = stbi_zlib_decode_noheader_malloc(reinterpret_cast<const char *>(data), static_cast<int>(size), &length);
        if (!bytes)
            return false;
        out.assign(bytes, bytes + length);
        std::free(bytes);
        return true;
    }

    // "VBoyColor-windows-v1.0.0/x/y" -> "x/y" ("" for the top folder itself).
    std::string WithoutTopFolder(std::string path)
    {
        for (char &c : path)
            if (c == '\\')
                c = '/';
        const size_t slash = path.find('/');
        return slash == std::string::npos ? "" : path.substr(slash + 1);
    }

    bool ReadZip(const std::vector<uint8_t> &zip, std::vector<PackageFile> &files)
    {
        if (zip.size() < 22)
            return false;
        // The end of central directory record (before a comment of up to 64 KB).
        size_t eocd = std::string::npos;
        for (size_t i = zip.size() - 22 + 1; i-- > 0 && zip.size() - i <= 22 + 65535;)
            if (Le32(&zip[i]) == 0x06054b50)
            {
                eocd = i;
                break;
            }
        if (eocd == std::string::npos)
            return false;
        const uint32_t count = Le16(&zip[eocd + 10]);
        size_t at = Le32(&zip[eocd + 16]);
        for (uint32_t n = 0; n < count; ++n)
        {
            if (at + 46 > zip.size() || Le32(&zip[at]) != 0x02014b50)
                return false;
            const uint32_t method = Le16(&zip[at + 10]);
            const uint32_t packed = Le32(&zip[at + 20]), size = Le32(&zip[at + 24]);
            const uint32_t nameLength = Le16(&zip[at + 28]), extra = Le16(&zip[at + 30]), comment = Le16(&zip[at + 32]);
            const uint32_t local = Le32(&zip[at + 42]);
            if (at + 46 + nameLength > zip.size())
                return false;
            const std::string name(reinterpret_cast<const char *>(&zip[at + 46]), nameLength);
            at += 46 + nameLength + extra + comment;
            if (name.empty() || name.back() == '/' || name.back() == '\\')
                continue; // (a folder)
            if (local + 30 > zip.size() || Le32(&zip[local]) != 0x04034b50)
                return false;
            const size_t data = local + 30 + Le16(&zip[local + 26]) + Le16(&zip[local + 28]);
            if (data + packed > zip.size())
                return false;
            PackageFile file;
            file.path = WithoutTopFolder(name);
            if (method == 0)
                file.data.assign(zip.begin() + data, zip.begin() + data + packed);
            else if (method != 8 || !Inflate(&zip[data], packed, file.data))
                return false;
            if (file.data.size() != size)
                return false;
            if (!file.path.empty())
                files.push_back(std::move(file));
        }
        return true;
    }

    bool ReadTarGz(const std::vector<uint8_t> &gz, std::vector<PackageFile> &files)
    {
        // gzip: a 10-byte header, optional fields, raw deflate, 8 bytes of CRC and size.
        if (gz.size() < 18 || gz[0] != 0x1f || gz[1] != 0x8b || gz[2] != 8)
            return false;
        const uint8_t flags = gz[3];
        size_t at = 10;
        if (flags & 4)
            at += 2 + Le16(&gz[at]);
        if (flags & 8)
            while (at < gz.size() && gz[at++])
                ;
        if (flags & 16)
            while (at < gz.size() && gz[at++])
                ;
        if (flags & 2)
            at += 2;
        std::vector<uint8_t> tar;
        if (at >= gz.size() - 8 || !Inflate(&gz[at], gz.size() - 8 - at, tar))
            return false;

        auto octal = [](const uint8_t *p, size_t n)
        {
            size_t v = 0;
            for (size_t i = 0; i < n && p[i]; ++i)
                if (p[i] >= '0' && p[i] <= '7')
                    v = v * 8 + (p[i] - '0');
            return v;
        };
        std::string longName;
        for (size_t block = 0; block + 512 <= tar.size();)
        {
            const uint8_t *h = &tar[block];
            if (h[0] == 0)
                break; // (the end)
            const size_t size = octal(h + 124, 12);
            const char type = static_cast<char>(h[156]);
            const size_t data = block + 512;
            if (data + size > tar.size())
                return false;
            std::string name(reinterpret_cast<const char *>(h), strnlen(reinterpret_cast<const char *>(h), 100));
            if (std::memcmp(h + 257, "ustar", 5) == 0 && h[345])
                name = std::string(reinterpret_cast<const char *>(h + 345), strnlen(reinterpret_cast<const char *>(h + 345), 155)) + "/" + name;
            if (type == 'L')
                longName.assign(reinterpret_cast<const char *>(&tar[data]), strnlen(reinterpret_cast<const char *>(&tar[data]), size));
            else if (type == '0' || type == '\0')
            {
                PackageFile file;
                file.path = WithoutTopFolder(longName.empty() ? name : longName);
                file.mode = static_cast<unsigned>(octal(h + 100, 8));
                file.data.assign(tar.begin() + data, tar.begin() + data + size);
                if (!file.path.empty())
                    files.push_back(std::move(file));
                longName.clear();
            }
            else if (type != 'x' && type != 'g')
                longName.clear();
            block = data + (size + 511) / 512 * 512;
        }
        return true;
    }

    std::filesystem::path U8(const std::string &s)
    {
#if defined(__cpp_char8_t)
        return std::filesystem::path(reinterpret_cast<const char8_t *>(s.c_str()));
#else
        return std::filesystem::u8path(s);
#endif
    }

    // Puts data at target: written beside it, then moved over it (whatever
    // is in use moved aside first, as <name>.old).
    bool Replace(const std::filesystem::path &target, const PackageFile &file)
    {
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        std::filesystem::path fresh = target;
        fresh += ".new";
        {
            std::ofstream out(fresh, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            out.write(reinterpret_cast<const char *>(file.data.data()), static_cast<std::streamsize>(file.data.size()));
            if (!out)
                return false;
        }
#if defined(_WIN32)
        if (std::filesystem::exists(target, ec))
        {
            std::filesystem::remove(target, ec);
            if (ec)
            {
                // In use (the running exe): out of the way.
                std::filesystem::path old = target;
                old += ".old";
                std::error_code oldEc;
                std::filesystem::remove(old, oldEc);
                std::filesystem::rename(target, old, ec);
                if (ec)
                    return false;
            }
        }
#else
        if (file.mode & 0111)
            chmod(fresh.c_str(), file.mode & 0777);
#endif
        std::filesystem::rename(fresh, target, ec); // (over the old one - a running binary keeps its own copy)
        return !ec;
    }
} // namespace

const char *DesktopPlatform::UpdatePackage() const
{
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "";
#endif
}

std::string DesktopPlatform::InstallUpdate(const std::vector<uint8_t> &package, bool &installed)
{
    installed = false;
    std::vector<PackageFile> files;
#if defined(_WIN32)
    const bool read = ReadZip(package, files);
    const char *exeName = "VBoyColor.exe";
#else
    const bool read = ReadTarGz(package, files);
    const char *exeName = "VBoyColor";
#endif
    bool hasApp = false;
    for (const PackageFile &file : files)
        hasApp |= file.path == exeName;
    if (!read || !hasApp)
        return "The download isn't a VBoy Color package - nothing changed";

    const std::filesystem::path app = U8(m_appFolder);
    int written = 0;
    for (const PackageFile &file : files)
    {
        if (file.path.find("..") != std::string::npos)
            continue;
        const std::filesystem::path target = app / U8(file.path);
        // The games folder that ships is just a note - never over the player's own.
        if (file.path.compare(0, 5, "roms/") == 0)
        {
            std::error_code ec;
            if (std::filesystem::exists(target, ec))
                continue;
        }
        if (!Replace(target, file))
            return written ? "The update stopped partway - get it from the release page" : "Can't write to VBoy Color's folder - nothing changed";
        ++written;
    }
    installed = true;
    RequestRestart();
    return "Updated - restarting";
}

void DesktopPlatform::Relaunch() const
{
    if (m_exePath.empty())
        return;
#if defined(_WIN32)
    const std::wstring exe = U8(m_exePath).wstring();
    std::wstring commandLine = L"\"" + exe + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, U8(m_appFolder).wstring().c_str(),
                       &startup, &process))
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
#else
    std::string exe = m_exePath;
    const std::string deleted = " (deleted)";
    if (exe.size() > deleted.size() && exe.compare(exe.size() - deleted.size(), deleted.size(), deleted) == 0)
        exe.erase(exe.size() - deleted.size());
    char *const argv[] = {exe.data(), nullptr};
    execv(exe.c_str(), argv);
#endif
}

void DesktopPlatform::OpenUrl(const std::string &url)
{
    if (url.compare(0, 8, "https://") != 0 || url.find_first_of("'\"`$\\ ") != std::string::npos)
        return;
#if defined(_WIN32)
    const std::wstring wide(url.begin(), url.end());
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open '" + url + "' &").c_str());
#else
    std::system(("xdg-open '" + url + "' >/dev/null 2>&1 &").c_str());
#endif
}
