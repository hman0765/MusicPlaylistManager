#include "playlist.h"

#include <windows.h>

#include <audioproperties.h>
#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>

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

std::wstring TagLibStringToWString(const TagLib::String& value)
{
    const std::string utf8 = value.to8Bit(true);
    if (utf8.empty())
    {
        return L"";
    }
    if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return L"";
    }
    const int utf8Length = static_cast<int>(utf8.size());
    const int wideLength = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), utf8Length, nullptr, 0);
    if (wideLength <= 0)
    {
        return L"";
    }
    std::wstring result(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            utf8.data(), utf8Length,
                            result.data(), wideLength) != wideLength)
    {
        return L"";
    }
    return result;
}

std::wstring GetPropertyValue(const TagLib::PropertyMap& properties,
                              const char* name)
{
    const TagLib::String key(name, TagLib::String::Latin1);
    if (!properties.contains(key) || properties[key].isEmpty())
    {
        return L"";
    }
    return TagLibStringToWString(properties[key].front());
}

std::wstring GetUppercaseFormat(const std::wstring& path)
{
    std::wstring extension = std::filesystem::path(path).extension().wstring();
    if (!extension.empty() && extension.front() == L'.')
    {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t character) { return std::towupper(character); });
    return extension;
}

std::wstring FormatFileTime(const FILETIME& utcTime)
{
    FILETIME localTime{};
    SYSTEMTIME systemTime{};
    if (!FileTimeToLocalFileTime(&utcTime, &localTime) ||
        !FileTimeToSystemTime(&localTime, &systemTime))
    {
        return L"";
    }
    wchar_t text[32]{};
    swprintf(text, std::size(text), L"%04u-%02u-%02u %02u:%02u",
             systemTime.wYear, systemTime.wMonth, systemTime.wDay,
             systemTime.wHour, systemTime.wMinute);
    return text;
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

std::string EncodeUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("The playlist data is too large.");
    }

    const int characterCount = static_cast<int>(text.size());
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), characterCount,
        nullptr, 0, nullptr, nullptr);
    if (byteCount == 0)
    {
        throw std::runtime_error("Failed to encode the playlist as UTF-8.");
    }

    std::string bytes(static_cast<std::size_t>(byteCount), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            text.data(), characterCount,
                            bytes.data(), byteCount,
                            nullptr, nullptr) == 0)
    {
        throw std::runtime_error("Failed to encode the playlist as UTF-8.");
    }
    return bytes;
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

    UpdateTrackMetadata(track);
    return track;
}

bool UpdateTrackMetadata(Track& track)
{
    MetadataRequest request{};
    request.title = request.artist = request.album = request.comment = true;
    request.duration = true;
    return UpdateTrackMetadata(track, request);
}

bool UpdateTrackMetadata(Track& track, const MetadataRequest& request)
{
    if (track.path.empty())
    {
        return false;
    }

    std::error_code pathError;
    if (!std::filesystem::is_regular_file(
            std::filesystem::path(track.path), pathError) || pathError)
    {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileData{};
    const bool needsFileProperties = request.fileSize || request.dateModified;
    if (needsFileProperties &&
        !GetFileAttributesExW(track.path.c_str(), GetFileExInfoStandard,
                             &fileData))
    {
        return false;
    }

    bool changedMetadata = false;
    const auto updateString = [&changedMetadata](std::wstring& destination,
                                                  const std::wstring& value) {
        if (destination != value)
        {
            destination = value;
            changedMetadata = true;
        }
    };

    const bool needsTag = request.title || request.artist || request.album ||
        request.comment || request.trackNumber || request.year ||
        request.genre || request.albumArtist || request.discNumber;
    const bool needsAudio = request.duration || request.bitrate ||
        request.sampleRate;
    if (needsTag || needsAudio)
    {
        TagLib::FileRef file{TagLib::FileName(track.path.c_str()),
                             needsAudio};
        if (!file.isNull())
        {
            if (const TagLib::Tag* tag = file.tag())
            {
                if (request.title)
                    updateString(track.title,
                                 TagLibStringToWString(tag->title()));
                if (request.artist)
                    updateString(track.artist,
                                 TagLibStringToWString(tag->artist()));
                if (request.album)
                    updateString(track.album,
                                 TagLibStringToWString(tag->album()));
                if (request.comment)
                    updateString(track.comment,
                                 TagLibStringToWString(tag->comment()));
                if (request.trackNumber)
                    updateString(track.trackNumber,
                                 tag->track() == 0 ? L"" :
                                 std::to_wstring(tag->track()));
                if (request.year)
                    updateString(track.year,
                                 tag->year() == 0 ? L"" :
                                 std::to_wstring(tag->year()));
                if (request.genre)
                    updateString(track.genre,
                                 TagLibStringToWString(tag->genre()));
            }
            if (request.albumArtist || request.discNumber)
            {
                const TagLib::PropertyMap properties = file.properties();
                if (request.albumArtist)
                    updateString(track.albumArtist,
                                 GetPropertyValue(properties, "ALBUMARTIST"));
                if (request.discNumber)
                    updateString(track.discNumber,
                                 GetPropertyValue(properties, "DISCNUMBER"));
            }
            if (needsAudio)
            {
                if (const TagLib::AudioProperties* properties =
                        file.audioProperties())
                {
                    if (request.duration)
                    {
                        const int seconds = properties->lengthInSeconds();
                        const std::wstring duration = FormatDuration(seconds);
                        if (track.durationSeconds != seconds ||
                            track.duration != duration)
                        {
                            track.durationSeconds = seconds;
                            track.duration = duration;
                            changedMetadata = true;
                        }
                    }
                    if (request.bitrate &&
                        track.bitrate != properties->bitrate())
                    {
                        track.bitrate = properties->bitrate();
                        changedMetadata = true;
                    }
                    if (request.sampleRate &&
                        track.sampleRate != properties->sampleRate())
                    {
                        track.sampleRate = properties->sampleRate();
                        changedMetadata = true;
                    }
                }
            }
        }
    }

    if (request.format)
    {
        updateString(track.format, GetUppercaseFormat(track.path));
    }
    if (request.fileSize)
    {
        const std::uint64_t size =
            (static_cast<std::uint64_t>(fileData.nFileSizeHigh) << 32) |
            fileData.nFileSizeLow;
        if (!track.hasFileSize || track.fileSize != size)
        {
            track.fileSize = size;
            track.hasFileSize = true;
            changedMetadata = true;
        }
    }
    if (request.dateModified)
    {
        updateString(track.dateModified,
                     FormatFileTime(fileData.ftLastWriteTime));
    }
    return changedMetadata;
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

namespace
{
enum class ExtinfField
{
    Title,
    Artist,
    Album,
    Comment,
    TrackNumber,
    Year,
    Genre,
    AlbumArtist,
    DiscNumber,
    Format,
    Bitrate,
    SampleRate,
    FileSize,
    DateModified,
    Count
};

struct ParsedExtinfFormat
{
    std::vector<ExtinfField> fields;
    std::size_t titleIndex = 0;
};

bool TryParseExtinfField(const std::wstring& name, ExtinfField& field)
{
    static constexpr const wchar_t* Names[] = {
        L"title", L"artist", L"album", L"comment", L"tracknumber",
        L"year", L"genre", L"albumartist", L"discnumber", L"format",
        L"bitrate", L"samplerate", L"filesize", L"datemodified"
    };
    for (std::size_t index = 0; index < std::size(Names); ++index)
    {
        if (name == Names[index])
        {
            field = static_cast<ExtinfField>(index);
            return true;
        }
    }
    return false;
}

bool ParseExtinfFormat(const std::wstring& format,
                       ParsedExtinfFormat& parsed,
                       std::wstring* errorMessage)
{
    const auto fail = [errorMessage](const wchar_t* message) {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return false;
    };
    if (format.empty())
        return fail(L"The format cannot be empty.");

    parsed = {};
    std::array<bool, static_cast<std::size_t>(ExtinfField::Count)> seen{};
    constexpr wchar_t Separator[] = L" - ";
    std::size_t start = 0;
    while (start <= format.size())
    {
        const std::size_t end = format.find(Separator, start);
        const std::wstring token = format.substr(
            start, end == std::wstring::npos ? std::wstring::npos
                                             : end - start);
        if (token.size() < 3 || token.front() != L'{' ||
            token.back() != L'}')
        {
            return fail(L"Use {field} items separated by ' - '.");
        }
        ExtinfField field{};
        if (!TryParseExtinfField(token.substr(1, token.size() - 2), field))
            return fail(L"The format contains an unknown field.");
        const std::size_t fieldIndex = static_cast<std::size_t>(field);
        if (seen[fieldIndex])
            return fail(L"Each field may appear only once.");
        seen[fieldIndex] = true;
        if (field == ExtinfField::Title)
            parsed.titleIndex = parsed.fields.size();
        parsed.fields.push_back(field);
        if (end == std::wstring::npos)
            break;
        start = end + std::size(Separator) - 1;
    }
    if (!seen[static_cast<std::size_t>(ExtinfField::Title)])
        return fail(L"The format must contain {title} exactly once.");
    if (errorMessage != nullptr)
        errorMessage->clear();
    return true;
}

std::wstring FormatExtinfFileSize(std::uint64_t bytes)
{
    static constexpr const wchar_t* Units[] = {
        L"B", L"KB", L"MB", L"GB", L"TB"
    };
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(Units))
    {
        value /= 1024.0;
        ++unit;
    }
    std::wostringstream text;
    if (unit == 0)
        text << bytes;
    else
        text << std::fixed << std::setprecision(1) << value;
    text << L' ' << Units[unit];
    return text.str();
}

std::wstring GetExtinfFieldValue(const Track& track, ExtinfField field,
                                 bool allowTitleFallback)
{
    switch (field)
    {
    case ExtinfField::Title:
        if (!track.title.empty() || !allowTitleFallback)
            return track.title;
        return track.path.empty()
            ? L"" : std::filesystem::path(track.path).stem().wstring();
    case ExtinfField::Artist: return track.artist;
    case ExtinfField::Album: return track.album;
    case ExtinfField::Comment: return track.comment;
    case ExtinfField::TrackNumber: return track.trackNumber;
    case ExtinfField::Year: return track.year;
    case ExtinfField::Genre: return track.genre;
    case ExtinfField::AlbumArtist: return track.albumArtist;
    case ExtinfField::DiscNumber: return track.discNumber;
    case ExtinfField::Format: return track.format;
    case ExtinfField::Bitrate:
        return track.bitrate < 0 ? L"" :
            std::to_wstring(track.bitrate) + L" kbps";
    case ExtinfField::SampleRate:
        return track.sampleRate < 0 ? L"" :
            std::to_wstring(track.sampleRate) + L" Hz";
    case ExtinfField::FileSize:
        return track.hasFileSize ? FormatExtinfFileSize(track.fileSize) : L"";
    case ExtinfField::DateModified: return track.dateModified;
    case ExtinfField::Count: break;
    }
    return L"";
}

std::wstring EvaluateExtinfFormat(const Track& track,
                                  const ParsedExtinfFormat& parsed)
{
    std::vector<std::wstring> output;
    for (std::size_t index = parsed.titleIndex; index > 0; --index)
    {
        std::wstring value = GetExtinfFieldValue(
            track, parsed.fields[index - 1], false);
        if (!value.empty())
        {
            output.push_back(std::move(value));
            break;
        }
    }
    std::wstring title = GetExtinfFieldValue(
        track, ExtinfField::Title, true);
    if (!title.empty())
        output.push_back(std::move(title));
    for (std::size_t index = parsed.titleIndex + 1;
         index < parsed.fields.size(); ++index)
    {
        std::wstring value = GetExtinfFieldValue(
            track, parsed.fields[index], false);
        if (!value.empty())
        {
            output.push_back(std::move(value));
            break;
        }
    }
    std::wstring result;
    for (const std::wstring& value : output)
    {
        if (!result.empty())
            result += L" - ";
        result += value;
    }
    return result;
}

std::wstring BuildExtinfText(const Track& track,
                             const ParsedExtinfFormat& parsed)
{
    const bool hasFormatValue = std::any_of(
        parsed.fields.begin(), parsed.fields.end(),
        [&track](ExtinfField field) {
            return !GetExtinfFieldValue(track, field, false).empty();
        });
    if (hasFormatValue)
        return EvaluateExtinfFormat(track, parsed);
    if (!track.extinfText.empty())
        return track.extinfText;
    return track.path.empty()
        ? L"" : std::filesystem::path(track.path).stem().wstring();
}
}

const wchar_t* GetExtinfFormatPresetName(ExtinfFormatPreset preset)
{
    switch (preset)
    {
    case ExtinfFormatPreset::ArtistTitle: return L"artistTitle";
    case ExtinfFormatPreset::Title: return L"title";
    case ExtinfFormatPreset::ArtistTitleAlbum: return L"artistTitleAlbum";
    case ExtinfFormatPreset::Custom: return L"custom";
    }
    return L"artistTitle";
}

bool TryParseExtinfFormatPreset(const std::wstring& name,
                                ExtinfFormatPreset& preset)
{
    for (const ExtinfFormatPreset candidate : {
         ExtinfFormatPreset::ArtistTitle, ExtinfFormatPreset::Title,
         ExtinfFormatPreset::ArtistTitleAlbum, ExtinfFormatPreset::Custom})
    {
        if (name == GetExtinfFormatPresetName(candidate))
        {
            preset = candidate;
            return true;
        }
    }
    return false;
}

std::wstring GetExtinfFormatString(ExtinfFormatPreset preset,
                                   const std::wstring& customFormat)
{
    switch (preset)
    {
    case ExtinfFormatPreset::ArtistTitle:
        return L"{artist} - {title}";
    case ExtinfFormatPreset::Title:
        return L"{title}";
    case ExtinfFormatPreset::ArtistTitleAlbum:
        return L"{artist} - {title} - {album}";
    case ExtinfFormatPreset::Custom:
        return customFormat;
    }
    return L"{artist} - {title}";
}

bool ValidateExtinfFormat(const std::wstring& format,
                          std::wstring* errorMessage)
{
    ParsedExtinfFormat parsed{};
    return ParseExtinfFormat(format, parsed, errorMessage);
}

std::wstring BuildExtinfText(const Track& track,
                             ExtinfFormatPreset preset,
                             const std::wstring& customFormat)
{
    ParsedExtinfFormat parsed{};
    std::wstring format = GetExtinfFormatString(preset, customFormat);
    if (!ParseExtinfFormat(format, parsed, nullptr))
        ParseExtinfFormat(DefaultCustomExtinfFormat, parsed, nullptr);
    return BuildExtinfText(track, parsed);
}

std::wstring BuildExtinfText(const Track& track)
{
    return BuildExtinfText(track, ExtinfFormatPreset::ArtistTitle,
                           DefaultCustomExtinfFormat);
}

int GetExportDuration(const Track& track)
{
    if (track.durationSeconds >= 0)
    {
        return track.durationSeconds;
    }
    if (track.extinfDuration >= 0)
    {
        return track.extinfDuration;
    }
    return -1;
}

void SaveM3U8(const Playlist& playlist, const std::wstring& filePath)
{
    SaveM3U8(playlist, filePath, ExtinfFormatPreset::ArtistTitle,
             DefaultCustomExtinfFormat);
}

void SaveM3U8(const Playlist& playlist, const std::wstring& filePath,
              ExtinfFormatPreset preset,
              const std::wstring& customFormat)
{
    ParsedExtinfFormat parsed{};
    if (!ParseExtinfFormat(GetExtinfFormatString(preset, customFormat),
                           parsed, nullptr))
        ParseExtinfFormat(DefaultCustomExtinfFormat, parsed, nullptr);
    std::wstring contents = L"#EXTM3U\r\n";
    for (const Track& track : playlist.tracks)
    {
        if (track.path.empty())
        {
            continue;
        }

        contents += L"#EXTINF:";
        contents += std::to_wstring(GetExportDuration(track));
        contents += L",";
        contents += BuildExtinfText(track, parsed);
        contents += L"\r\n";
        contents += track.path;
        contents += L"\r\n";
    }

    const std::string utf8 = EncodeUtf8(contents);
    std::ofstream file(std::filesystem::path(filePath),
                       std::ios::binary | std::ios::trunc);
    if (!file)
    {
        throw std::runtime_error("Failed to create the m3u8 file.");
    }
    file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    if (!file)
    {
        throw std::runtime_error("Failed to write the m3u8 file.");
    }
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

namespace
{
bool HasVisibleGroupName(const std::wstring& name)
{
    return std::any_of(name.begin(), name.end(), [](wchar_t character) {
        return std::iswspace(character) == 0;
    });
}

bool EqualGroupNames(const std::wstring& left, const std::wstring& right)
{
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(),
                   [](wchar_t first, wchar_t second) {
                       return std::towlower(first) == std::towlower(second);
                   });
}
}

PlaylistGroup* FindPlaylistGroupById(
    std::vector<PlaylistGroup>& groups, int groupId)
{
    const auto found = std::find_if(
        groups.begin(), groups.end(), [groupId](const PlaylistGroup& group) {
            return group.id == groupId;
        });
    return found == groups.end() ? nullptr : &*found;
}

const PlaylistGroup* FindPlaylistGroupById(
    const std::vector<PlaylistGroup>& groups, int groupId)
{
    const auto found = std::find_if(
        groups.begin(), groups.end(), [groupId](const PlaylistGroup& group) {
            return group.id == groupId;
        });
    return found == groups.end() ? nullptr : &*found;
}

bool IsPlaylistGroupNameAvailable(
    const std::vector<PlaylistGroup>& groups, const std::wstring& name)
{
    if (!HasVisibleGroupName(name) || EqualGroupNames(name, L"New"))
    {
        return false;
    }
    return std::none_of(
        groups.begin(), groups.end(), [&name](const PlaylistGroup& group) {
            return EqualGroupNames(group.name, name);
        });
}

std::wstring GenerateNewGroupName(
    const std::vector<PlaylistGroup>& groups)
{
    const std::wstring baseName = L"New Group";
    if (IsPlaylistGroupNameAvailable(groups, baseName))
    {
        return baseName;
    }
    for (int number = 2;; ++number)
    {
        const std::wstring candidate =
            baseName + L" " + std::to_wstring(number);
        if (IsPlaylistGroupNameAvailable(groups, candidate))
        {
            return candidate;
        }
    }
}

int CreatePlaylistGroup(std::vector<PlaylistGroup>& groups,
                        int& nextGroupId, const std::wstring& name)
{
    if (!IsPlaylistGroupNameAvailable(groups, name))
    {
        return -1;
    }
    nextGroupId = std::max(nextGroupId, 1);
    while (FindPlaylistGroupById(groups, nextGroupId) != nullptr)
    {
        ++nextGroupId;
    }
    const int newId = nextGroupId++;
    groups.push_back({newId, name, true});
    return newId;
}

void NormalizePlaylistGroups(std::vector<PlaylistGroup>& groups,
                             std::vector<Playlist>& playlists,
                             int& nextGroupId)
{
    auto newGroup = std::find_if(
        groups.begin(), groups.end(), [](const PlaylistGroup& group) {
            return group.id == NewPlaylistGroupId;
        });
    if (newGroup == groups.end())
    {
        groups.insert(groups.begin(),
                      {NewPlaylistGroupId, L"New", true});
    }
    else
    {
        newGroup->name = L"New";
        if (newGroup != groups.begin())
        {
            std::rotate(groups.begin(), newGroup, newGroup + 1);
        }
    }

    // Repair duplicate or reserved IDs without tying identity to vector order.
    int availableId = 1;
    for (std::size_t index = 1; index < groups.size(); ++index)
    {
        PlaylistGroup& group = groups[index];
        const bool duplicateId = std::any_of(
            groups.begin(), groups.begin() + static_cast<std::ptrdiff_t>(index),
            [&group](const PlaylistGroup& previous) {
                return previous.id == group.id;
            });
        if (group.id <= NewPlaylistGroupId || duplicateId)
        {
            while (FindPlaylistGroupById(groups, availableId) != nullptr)
            {
                ++availableId;
            }
            group.id = availableId++;
        }
    }

    std::vector<PlaylistGroup> groupsWithValidNames;
    groupsWithValidNames.reserve(groups.size());
    groupsWithValidNames.push_back(groups.front());
    for (std::size_t index = 1; index < groups.size(); ++index)
    {
        PlaylistGroup& group = groups[index];
        if (!IsPlaylistGroupNameAvailable(groupsWithValidNames, group.name))
        {
            group.name = GenerateNewGroupName(groupsWithValidNames);
        }
        groupsWithValidNames.push_back(group);
    }

    nextGroupId = 1;
    for (const PlaylistGroup& group : groups)
    {
        if (group.id >= nextGroupId)
        {
            nextGroupId = group.id + 1;
        }
    }
    for (Playlist& playlist : playlists)
    {
        if (FindPlaylistGroupById(groups, playlist.groupId) == nullptr)
        {
            playlist.groupId = NewPlaylistGroupId;
        }
    }
}
