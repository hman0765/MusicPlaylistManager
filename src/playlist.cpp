#include "playlist.h"

#include <algorithm>
#include <array>
#include <cwctype>

namespace
{
std::wstring FileNameFromPath(const std::wstring& path)
{
    const auto separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}
}

bool IsSupportedAudioPath(const std::wstring& path)
{
    const auto separator = path.find_last_of(L"\\/");
    const auto dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos ||
        (separator != std::wstring::npos && dot < separator))
    {
        return false;
    }

    std::wstring extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t character) { return std::towlower(character); });

    static constexpr std::array<const wchar_t*, 5> supportedExtensions = {
        L".mp3", L".flac", L".wav", L".m4a", L".ogg"
    };

    return std::any_of(supportedExtensions.begin(), supportedExtensions.end(),
                       [&extension](const wchar_t* supported) {
                           return extension == supported;
                       });
}

void AddTrack(Playlist& playlist, const std::wstring& path)
{
    playlist.tracks.push_back(Track{path, FileNameFromPath(path)});
    playlist.isModified = true;
}
