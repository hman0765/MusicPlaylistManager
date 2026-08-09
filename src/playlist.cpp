#include "playlist.h"

#include <windows.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
std::wstring LowercaseExtension(const std::wstring& path)
{
    const auto separator = path.find_last_of(L"\\/");
    const auto dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos ||
        (separator != std::wstring::npos && dot < separator))
    {
        return L"";
    }

    std::wstring extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t character) { return std::towlower(character); });
    return extension;
}

std::wstring ReadStringProperty(IPropertyStore* propertyStore,
                                REFPROPERTYKEY propertyKey)
{
    PROPVARIANT value{};
    PropVariantInit(&value);

    std::wstring result;
    if (SUCCEEDED(propertyStore->GetValue(propertyKey, &value)))
    {
        wchar_t text[1024]{};
        if (SUCCEEDED(PropVariantToString(value, text,
                                          static_cast<UINT>(std::size(text)))))
        {
            result = text;
        }
    }

    PropVariantClear(&value);
    return result;
}

std::wstring ReadDurationProperty(IPropertyStore* propertyStore)
{
    PROPVARIANT value{};
    PropVariantInit(&value);

    ULONGLONG durationInHundredNanoseconds = 0;
    const bool hasDuration =
        SUCCEEDED(propertyStore->GetValue(PKEY_Media_Duration, &value)) &&
        SUCCEEDED(PropVariantToUInt64(value, &durationInHundredNanoseconds)) &&
        durationInHundredNanoseconds > 0;
    PropVariantClear(&value);

    if (!hasDuration)
    {
        return L"";
    }

    constexpr ULONGLONG HundredNanosecondsPerSecond = 10'000'000;
    const ULONGLONG totalSeconds =
        durationInHundredNanoseconds / HundredNanosecondsPerSecond;
    const ULONGLONG hours = totalSeconds / 3600;
    const ULONGLONG minutes = (totalSeconds / 60) % 60;
    const ULONGLONG seconds = totalSeconds % 60;

    std::wostringstream stream;
    stream << std::setfill(L'0');
    if (hours > 0)
    {
        stream << hours << L':' << std::setw(2) << minutes;
    }
    else
    {
        stream << std::setw(2) << minutes;
    }
    stream << L':' << std::setw(2) << seconds;
    return stream.str();
}

std::wstring DecodeUtf8(std::string bytes)
{
    constexpr unsigned char Utf8Bom[] = {0xEF, 0xBB, 0xBF};
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == Utf8Bom[0] &&
        static_cast<unsigned char>(bytes[1]) == Utf8Bom[1] &&
        static_cast<unsigned char>(bytes[2]) == Utf8Bom[2])
    {
        bytes.erase(0, 3);
    }

    if (bytes.empty())
    {
        return L"";
    }
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("The m3u8 file is too large.");
    }

    const int byteCount = static_cast<int>(bytes.size());
    const int characterCount = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), byteCount, nullptr, 0);
    if (characterCount == 0)
    {
        throw std::runtime_error("The m3u8 file is not valid UTF-8.");
    }

    std::wstring text(static_cast<std::size_t>(characterCount), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            bytes.data(), byteCount,
                            text.data(), characterCount) == 0)
    {
        throw std::runtime_error("Failed to decode the m3u8 file.");
    }
    return text;
}

std::wstring ReadM3U8Text(const std::wstring& filePath)
{
    std::ifstream file(std::filesystem::path(filePath),
                       std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open the m3u8 file.");
    }

    std::string bytes((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    if (file.bad())
    {
        throw std::runtime_error("Failed to read the m3u8 file.");
    }
    return DecodeUtf8(std::move(bytes));
}

std::wstring TrimWhitespace(const std::wstring& text)
{
    const auto first = std::find_if_not(text.begin(), text.end(),
                                        [](wchar_t character) {
                                            return std::iswspace(character) != 0;
                                        });
    const auto last = std::find_if_not(text.rbegin(), text.rend(),
                                       [](wchar_t character) {
                                           return std::iswspace(character) != 0;
                                       }).base();
    return first < last ? std::wstring(first, last) : L"";
}

int ParseExtinfDuration(const std::wstring& text)
{
    const std::wstring trimmed = TrimWhitespace(text);
    if (trimmed.empty())
    {
        return -1;
    }

    try
    {
        std::size_t parsedCharacters = 0;
        const long long value = std::stoll(trimmed, &parsedCharacters, 10);
        if (parsedCharacters != trimmed.size() || value < 0 ||
            value > std::numeric_limits<int>::max())
        {
            return -1;
        }
        return static_cast<int>(value);
    }
    catch (const std::exception&)
    {
        return -1;
    }
}
}

bool IsSupportedAudioPath(const std::wstring& path)
{
    const std::wstring extension = LowercaseExtension(path);

    static constexpr std::array<const wchar_t*, 5> supportedExtensions = {
        L".mp3", L".flac", L".wav", L".m4a", L".ogg"
    };

    return std::any_of(supportedExtensions.begin(), supportedExtensions.end(),
                       [&extension](const wchar_t* supported) {
                           return extension == supported;
                       });
}

bool IsM3U8Path(const std::wstring& path)
{
    return LowercaseExtension(path) == L".m3u8";
}

Track CreateTrackFromFile(const std::wstring& path)
{
    Track track{};
    track.path = path;

    IPropertyStore* propertyStore = nullptr;
    const HRESULT result = SHGetPropertyStoreFromParsingName(
        path.c_str(), nullptr, GPS_BESTEFFORT,
        IID_PPV_ARGS(&propertyStore));
    if (SUCCEEDED(result))
    {
        track.title = ReadStringProperty(propertyStore, PKEY_Title);
        track.artist = ReadStringProperty(propertyStore, PKEY_Music_Artist);
        track.album = ReadStringProperty(propertyStore, PKEY_Music_AlbumTitle);
        track.duration = ReadDurationProperty(propertyStore);
        propertyStore->Release();
    }

    return track;
}

Playlist LoadM3U8(const std::wstring& filePath)
{
    const std::filesystem::path playlistPath(filePath);
    Playlist playlist{};
    playlist.name = playlistPath.stem().wstring();
    playlist.filePath = filePath;

    const std::wstring contents = ReadM3U8Text(filePath);
    std::wistringstream lines(contents);
    std::wstring line;
    std::wstring pendingExtinfText;
    int pendingExtinfDuration = -1;

    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }

        constexpr wchar_t ExtinfPrefix[] = L"#EXTINF:";
        if (line.rfind(ExtinfPrefix, 0) == 0)
        {
            const std::wstring value = line.substr(std::size(ExtinfPrefix) - 1);
            const auto comma = value.find(L',');
            const std::wstring durationText = value.substr(0, comma);
            pendingExtinfDuration = ParseExtinfDuration(durationText);
            pendingExtinfText = comma == std::wstring::npos
                ? L""
                : value.substr(comma + 1);
            continue;
        }
        if (line.front() == L'#')
        {
            continue;
        }

        std::filesystem::path trackPath(line);
        if (trackPath.is_relative())
        {
            trackPath = playlistPath.parent_path() / trackPath;
        }

        Track track{};
        track.path = trackPath.lexically_normal().wstring();
        track.extinfText = pendingExtinfText;
        track.extinfDuration = pendingExtinfDuration;
        playlist.tracks.push_back(std::move(track));

        pendingExtinfText.clear();
        pendingExtinfDuration = -1;
    }

    playlist.isModified = false;
    return playlist;
}

std::wstring FormatDuration(int totalSeconds)
{
    if (totalSeconds < 0)
    {
        return L"";
    }

    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;

    std::wostringstream stream;
    stream << std::setfill(L'0');
    if (hours > 0)
    {
        stream << std::setw(2) << hours << L':'
               << std::setw(2) << minutes << L':'
               << std::setw(2) << seconds;
    }
    else
    {
        stream << std::setw(2) << minutes << L':'
               << std::setw(2) << seconds;
    }
    return stream.str();
}

void AddTrack(Playlist& playlist, Track track)
{
    playlist.tracks.push_back(std::move(track));
    playlist.isModified = true;
}
