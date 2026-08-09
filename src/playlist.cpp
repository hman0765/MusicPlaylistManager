#include "playlist.h"

#include <windows.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
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

Track CreateTrackFromFile(const std::wstring& path)
{
    Track track{L"", L"", L"", L"", path};

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

void AddTrack(Playlist& playlist, Track track)
{
    playlist.tracks.push_back(std::move(track));
    playlist.isModified = true;
}
