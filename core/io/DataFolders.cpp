#include "io/DataFolders.h"

#include <algorithm>
#include <cctype>
#include <string>

DataKind DataKindOf(const std::string &fileName, bool inStatesDir)
{
    if (fileName.compare(0, std::char_traits<char>::length(kScreenshotsPrefix), kScreenshotsPrefix) == 0)
        return DataKind::Screenshots;
    if (fileName.find('/') != std::string::npos)
        return DataKind::Games; // (captures/, recordings/, colorpacks/...)
    std::string ext;
    if (const size_t dot = fileName.rfind('.'); dot != std::string::npos)
        ext = fileName.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!inStatesDir)
        return ext == "srm" ? DataKind::Saves : DataKind::Games;
    // "<game>.state", ".state2", ".stateimg", ".statepng3"... (Emulator::StateFileName)
    return ext.compare(0, 5, "state") == 0 ? DataKind::States : DataKind::Settings;
}

std::string NameInFolder(const std::string &fileName, DataKind kind)
{
    const size_t prefix = std::char_traits<char>::length(kScreenshotsPrefix);
    if (kind == DataKind::Screenshots && fileName.compare(0, prefix, kScreenshotsPrefix) == 0)
        return fileName.substr(prefix);
    return fileName;
}
