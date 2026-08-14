#include "app_state.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

std::vector<TrackColumnConfig> MakeDefaultTrackColumnConfigs()
{
    return {
        {TrackColumnId::Title, true, 180},
        {TrackColumnId::Artist, true, 140},
        {TrackColumnId::Album, true, 160},
        {TrackColumnId::Duration, true, 85},
        {TrackColumnId::Comment, false, 180},
        {TrackColumnId::Path, false, 240},
        {TrackColumnId::TrackNumber, false, 90},
        {TrackColumnId::Year, false, 75},
        {TrackColumnId::Genre, false, 140},
        {TrackColumnId::AlbumArtist, false, 160},
        {TrackColumnId::DiscNumber, false, 90},
        {TrackColumnId::Format, false, 80},
        {TrackColumnId::Bitrate, false, 95},
        {TrackColumnId::SampleRate, false, 110},
        {TrackColumnId::FileSize, false, 100},
        {TrackColumnId::DateModified, false, 145}
    };
}

const wchar_t* GetTrackColumnIdName(TrackColumnId id)
{
    static constexpr const wchar_t* Names[] = {
        L"title", L"artist", L"album", L"duration", L"comment", L"path",
        L"trackNumber", L"year", L"genre", L"albumArtist", L"discNumber",
        L"format", L"bitrate", L"sampleRate", L"fileSize", L"dateModified"
    };
    const auto index = static_cast<std::size_t>(id);
    return index < std::size(Names) ? Names[index] : L"";
}

bool TryParseTrackColumnId(const std::wstring& name, TrackColumnId& id)
{
    for (const TrackColumnConfig& config : MakeDefaultTrackColumnConfigs())
    {
        if (name == GetTrackColumnIdName(config.id))
        {
            id = config.id;
            return true;
        }
    }
    return false;
}

namespace
{
constexpr std::uintmax_t MaximumStateFileSize = 128ULL * 1024ULL * 1024ULL;

std::string EncodeUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("State text is too large.");
    }

    const int characterCount = static_cast<int>(text.size());
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), characterCount,
        nullptr, 0, nullptr, nullptr);
    if (byteCount == 0)
    {
        throw std::runtime_error("State contains invalid Unicode.");
    }

    std::string bytes(static_cast<std::size_t>(byteCount), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            text.data(), characterCount,
                            bytes.data(), byteCount, nullptr, nullptr) == 0)
    {
        throw std::runtime_error("State UTF-8 encoding failed.");
    }
    return bytes;
}

std::wstring DecodeUtf8(std::string bytes)
{
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF)
    {
        bytes.erase(0, 3);
    }
    if (bytes.empty())
    {
        return L"";
    }
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("State file is too large.");
    }

    const int byteCount = static_cast<int>(bytes.size());
    const int characterCount = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), byteCount, nullptr, 0);
    if (characterCount == 0)
    {
        throw std::runtime_error("State file is not valid UTF-8.");
    }

    std::wstring text(static_cast<std::size_t>(characterCount), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            bytes.data(), byteCount,
                            text.data(), characterCount) == 0)
    {
        throw std::runtime_error("State UTF-8 decoding failed.");
    }
    return text;
}

void AppendHexEscape(std::wstring& output, wchar_t character)
{
    constexpr wchar_t HexDigits[] = L"0123456789abcdef";
    output += L"\\u";
    output += HexDigits[(character >> 12) & 0x0F];
    output += HexDigits[(character >> 8) & 0x0F];
    output += HexDigits[(character >> 4) & 0x0F];
    output += HexDigits[character & 0x0F];
}

void AppendJsonString(std::wstring& output, const std::wstring& value)
{
    output += L'"';
    for (const wchar_t character : value)
    {
        switch (character)
        {
        case L'"': output += L"\\\""; break;
        case L'\\': output += L"\\\\"; break;
        case L'\b': output += L"\\b"; break;
        case L'\f': output += L"\\f"; break;
        case L'\n': output += L"\\n"; break;
        case L'\r': output += L"\\r"; break;
        case L'\t': output += L"\\t"; break;
        default:
            if (character < 0x20)
            {
                AppendHexEscape(output, character);
            }
            else
            {
                output += character;
            }
        }
    }
    output += L'"';
}

void AppendIndent(std::wstring& output, int level)
{
    output.append(static_cast<std::size_t>(level * 2), L' ');
}

void AppendTrackJson(std::wstring& output, const Track& track, int indent)
{
    output += L"{\n";
    const auto appendStringField = [&](const wchar_t* name,
                                       const std::wstring& value,
                                       bool trailingComma) {
        AppendIndent(output, indent + 1);
        AppendJsonString(output, name);
        output += L": ";
        AppendJsonString(output, value);
        output += trailingComma ? L",\n" : L"\n";
    };
    appendStringField(L"title", track.title, true);
    appendStringField(L"artist", track.artist, true);
    appendStringField(L"album", track.album, true);
    appendStringField(L"comment", track.comment, true);
    appendStringField(L"duration", track.duration, true);
    AppendIndent(output, indent + 1);
    output += L"\"durationSeconds\": " +
              std::to_wstring(track.durationSeconds) + L",\n";
    appendStringField(L"path", track.path, true);
    appendStringField(L"extinfText", track.extinfText, true);
    AppendIndent(output, indent + 1);
    output += L"\"extinfDuration\": " +
              std::to_wstring(track.extinfDuration) + L",\n";
    appendStringField(L"trackNumber", track.trackNumber, true);
    appendStringField(L"year", track.year, true);
    appendStringField(L"genre", track.genre, true);
    appendStringField(L"albumArtist", track.albumArtist, true);
    appendStringField(L"discNumber", track.discNumber, true);
    appendStringField(L"format", track.format, true);
    AppendIndent(output, indent + 1);
    output += L"\"bitrate\": " + std::to_wstring(track.bitrate) + L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"sampleRate\": " + std::to_wstring(track.sampleRate) + L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"fileSize\": " + std::to_wstring(track.fileSize) + L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"hasFileSize\": ";
    output += track.hasFileSize ? L"true,\n" : L"false,\n";
    appendStringField(L"dateModified", track.dateModified, false);
    AppendIndent(output, indent);
    output += L"}";
}

void AppendPlaylistJson(std::wstring& output, const Playlist& playlist,
                        int indent)
{
    output += L"{\n";
    AppendIndent(output, indent + 1);
    output += L"\"name\": ";
    AppendJsonString(output, playlist.name);
    output += L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"filePath\": ";
    AppendJsonString(output, playlist.filePath);
    output += L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"groupId\": " + std::to_wstring(playlist.groupId) +
              L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"isModified\": ";
    output += playlist.isModified ? L"true,\n" : L"false,\n";
    AppendIndent(output, indent + 1);
    output += L"\"tracks\": [";
    if (!playlist.tracks.empty())
    {
        output += L"\n";
        for (std::size_t index = 0; index < playlist.tracks.size(); ++index)
        {
            AppendIndent(output, indent + 2);
            AppendTrackJson(output, playlist.tracks[index], indent + 2);
            output += index + 1 < playlist.tracks.size() ? L",\n" : L"\n";
        }
        AppendIndent(output, indent + 1);
    }
    output += L"]\n";
    AppendIndent(output, indent);
    output += L"}";
}

void AppendPlaylistGroupJson(std::wstring& output,
                             const PlaylistGroup& group, int indent)
{
    output += L"{\n";
    AppendIndent(output, indent + 1);
    output += L"\"id\": " + std::to_wstring(group.id) + L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"name\": ";
    AppendJsonString(output, group.name);
    output += L",\n";
    AppendIndent(output, indent + 1);
    output += L"\"expanded\": ";
    output += group.expanded ? L"true\n" : L"false\n";
    AppendIndent(output, indent);
    output += L"}";
}

void AppendSendToApplicationJson(std::wstring& output,
                                 const SendToApplication& application,
                                 int indent)
{
    output += L"{\n";
    const auto appendField = [&](const wchar_t* name,
                                 const std::wstring& value,
                                 bool trailingComma)
    {
        AppendIndent(output, indent + 1);
        AppendJsonString(output, name);
        output += L": ";
        AppendJsonString(output, value);
        output += trailingComma ? L",\n" : L"\n";
    };
    appendField(L"name", application.name, true);
    appendField(L"executablePath", application.executablePath, true);
    appendField(L"arguments", application.arguments, false);
    AppendIndent(output, indent);
    output += L"}";
}

std::wstring SerializeState(const AppState& state)
{
    std::wstring output = L"{\n";
    output += L"  \"version\": " + std::to_wstring(state.version) + L",\n";
    output += L"  \"selectedPlaylistIndex\": " +
              std::to_wstring(state.selectedPlaylistIndex) + L",\n";
    output += L"  \"gui\": {\n";
    output += L"    \"splitterX\": " + std::to_wstring(state.splitterX) + L",\n";
    output += L"    \"windowX\": " + std::to_wstring(state.windowX) + L",\n";
    output += L"    \"windowY\": " + std::to_wstring(state.windowY) + L",\n";
    output += L"    \"windowWidth\": " +
              std::to_wstring(state.windowWidth) + L",\n";
    output += L"    \"windowHeight\": " +
              std::to_wstring(state.windowHeight) + L",\n";
    output += L"    \"trackColumns\": [";
    if (!state.trackColumns.empty())
    {
        output += L"\n";
        for (std::size_t index = 0; index < state.trackColumns.size(); ++index)
        {
            const TrackColumnConfig& column = state.trackColumns[index];
            output += L"      {\"id\": ";
            AppendJsonString(output, GetTrackColumnIdName(column.id));
            output += L", \"visible\": ";
            output += column.visible ? L"true" : L"false";
            output += L", \"width\": " + std::to_wstring(column.width) + L"}";
            output += index + 1 < state.trackColumns.size() ? L",\n" : L"\n";
        }
        output += L"    ";
    }
    output += L"]\n";
    output += L"  },\n";
    output += L"  \"groups\": [";
    if (!state.playlistGroups.empty())
    {
        output += L"\n";
        for (std::size_t index = 0; index < state.playlistGroups.size();
             ++index)
        {
            output += L"    ";
            AppendPlaylistGroupJson(output, state.playlistGroups[index], 2);
            output += index + 1 < state.playlistGroups.size()
                ? L",\n" : L"\n";
        }
        output += L"  ";
    }
    output += L"],\n";
    output += L"  \"sendToApplications\": [";
    if (!state.sendToApplications.empty())
    {
        output += L"\n";
        const std::size_t count = std::min(
            state.sendToApplications.size(), MaximumSendToApplications);
        for (std::size_t index = 0; index < count; ++index)
        {
            output += L"    ";
            AppendSendToApplicationJson(
                output, state.sendToApplications[index], 2);
            output += index + 1 < count ? L",\n" : L"\n";
        }
        output += L"  ";
    }
    output += L"],\n";
    output += L"  \"playlists\": [";
    if (!state.playlists.empty())
    {
        output += L"\n";
        for (std::size_t index = 0; index < state.playlists.size(); ++index)
        {
            output += L"    ";
            AppendPlaylistJson(output, state.playlists[index], 2);
            output += index + 1 < state.playlists.size() ? L",\n" : L"\n";
        }
        output += L"  ";
    }
    output += L"]\n}\n";
    return output;
}

class JsonReader
{
public:
    explicit JsonReader(const std::wstring& text) : text_(text) {}

    void ReadObject(const std::function<void(const std::wstring&)>& readMember)
    {
        Expect(L'{');
        SkipWhitespace();
        if (Consume(L'}'))
        {
            return;
        }
        while (true)
        {
            const std::wstring name = ReadString();
            Expect(L':');
            readMember(name);
            SkipWhitespace();
            if (Consume(L'}'))
            {
                return;
            }
            Expect(L',');
        }
    }

    void ReadArray(const std::function<void()>& readElement)
    {
        Expect(L'[');
        SkipWhitespace();
        if (Consume(L']'))
        {
            return;
        }
        while (true)
        {
            readElement();
            SkipWhitespace();
            if (Consume(L']'))
            {
                return;
            }
            Expect(L',');
        }
    }

    std::wstring ReadString()
    {
        SkipWhitespace();
        if (position_ >= text_.size() || text_[position_++] != L'"')
        {
            throw std::runtime_error("Expected JSON string.");
        }

        std::wstring result;
        while (position_ < text_.size())
        {
            const wchar_t character = text_[position_++];
            if (character == L'"')
            {
                return result;
            }
            if (character < 0x20)
            {
                throw std::runtime_error("Unescaped JSON control character.");
            }
            if (character != L'\\')
            {
                result += character;
                continue;
            }
            if (position_ >= text_.size())
            {
                throw std::runtime_error("Incomplete JSON escape.");
            }
            switch (text_[position_++])
            {
            case L'"': result += L'"'; break;
            case L'\\': result += L'\\'; break;
            case L'/': result += L'/'; break;
            case L'b': result += L'\b'; break;
            case L'f': result += L'\f'; break;
            case L'n': result += L'\n'; break;
            case L'r': result += L'\r'; break;
            case L't': result += L'\t'; break;
            case L'u': ReadUnicodeEscape(result); break;
            default: throw std::runtime_error("Invalid JSON escape.");
            }
        }
        throw std::runtime_error("Unterminated JSON string.");
    }

    long long ReadInteger()
    {
        const std::wstring token = ReadNumberToken();
        if (token.find_first_of(L".eE") != std::wstring::npos)
        {
            throw std::runtime_error("Expected JSON integer.");
        }
        std::size_t parsed = 0;
        const long long value = std::stoll(token, &parsed, 10);
        if (parsed != token.size())
        {
            throw std::runtime_error("Invalid JSON integer.");
        }
        return value;
    }

    bool ReadBoolean()
    {
        SkipWhitespace();
        if (MatchLiteral(L"true"))
        {
            return true;
        }
        if (MatchLiteral(L"false"))
        {
            return false;
        }
        throw std::runtime_error("Expected JSON boolean.");
    }

    void SkipValue(int depth = 0)
    {
        if (depth > 128)
        {
            throw std::runtime_error("JSON nesting is too deep.");
        }
        SkipWhitespace();
        if (position_ >= text_.size())
        {
            throw std::runtime_error("Missing JSON value.");
        }
        const wchar_t character = text_[position_];
        if (character == L'"')
        {
            ReadString();
        }
        else if (character == L'{')
        {
            ReadObject([&](const std::wstring&) { SkipValue(depth + 1); });
        }
        else if (character == L'[')
        {
            ReadArray([&]() { SkipValue(depth + 1); });
        }
        else if (character == L't' && MatchLiteral(L"true")) {}
        else if (character == L'f' && MatchLiteral(L"false")) {}
        else if (character == L'n' && MatchLiteral(L"null")) {}
        else
        {
            ReadNumberToken();
        }
    }

    void EnsureEnd()
    {
        SkipWhitespace();
        if (position_ != text_.size())
        {
            throw std::runtime_error("Unexpected data after JSON document.");
        }
    }

private:
    void SkipWhitespace()
    {
        while (position_ < text_.size() &&
               (text_[position_] == L' ' || text_[position_] == L'\t' ||
                text_[position_] == L'\r' || text_[position_] == L'\n'))
        {
            ++position_;
        }
    }

    bool Consume(wchar_t expected)
    {
        SkipWhitespace();
        if (position_ < text_.size() && text_[position_] == expected)
        {
            ++position_;
            return true;
        }
        return false;
    }

    void Expect(wchar_t expected)
    {
        if (!Consume(expected))
        {
            throw std::runtime_error("Unexpected JSON token.");
        }
    }

    bool MatchLiteral(const wchar_t* literal)
    {
        SkipWhitespace();
        const std::size_t length = std::wcslen(literal);
        if (text_.compare(position_, length, literal) != 0)
        {
            return false;
        }
        position_ += length;
        return true;
    }

    unsigned int ReadHex4()
    {
        if (position_ + 4 > text_.size())
        {
            throw std::runtime_error("Incomplete JSON Unicode escape.");
        }
        unsigned int value = 0;
        for (int index = 0; index < 4; ++index)
        {
            const wchar_t digit = text_[position_++];
            value <<= 4;
            if (digit >= L'0' && digit <= L'9') value += digit - L'0';
            else if (digit >= L'a' && digit <= L'f') value += digit - L'a' + 10;
            else if (digit >= L'A' && digit <= L'F') value += digit - L'A' + 10;
            else throw std::runtime_error("Invalid JSON Unicode escape.");
        }
        return value;
    }

    void ReadUnicodeEscape(std::wstring& output)
    {
        const unsigned int first = ReadHex4();
        if (first >= 0xD800 && first <= 0xDBFF)
        {
            if (position_ + 2 > text_.size() ||
                text_[position_] != L'\\' || text_[position_ + 1] != L'u')
            {
                throw std::runtime_error("Missing low surrogate.");
            }
            position_ += 2;
            const unsigned int second = ReadHex4();
            if (second < 0xDC00 || second > 0xDFFF)
            {
                throw std::runtime_error("Invalid low surrogate.");
            }
            output += static_cast<wchar_t>(first);
            output += static_cast<wchar_t>(second);
        }
        else if (first >= 0xDC00 && first <= 0xDFFF)
        {
            throw std::runtime_error("Unexpected low surrogate.");
        }
        else
        {
            output += static_cast<wchar_t>(first);
        }
    }

    std::wstring ReadNumberToken()
    {
        SkipWhitespace();
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == L'-') ++position_;
        if (position_ >= text_.size())
            throw std::runtime_error("Invalid JSON number.");
        if (text_[position_] == L'0')
        {
            ++position_;
        }
        else if (text_[position_] >= L'1' && text_[position_] <= L'9')
        {
            while (position_ < text_.size() &&
                   text_[position_] >= L'0' && text_[position_] <= L'9')
                ++position_;
        }
        else
        {
            throw std::runtime_error("Invalid JSON number.");
        }
        if (position_ < text_.size() && text_[position_] == L'.')
        {
            ++position_;
            const std::size_t fractionStart = position_;
            while (position_ < text_.size() &&
                   text_[position_] >= L'0' && text_[position_] <= L'9')
                ++position_;
            if (position_ == fractionStart)
                throw std::runtime_error("Invalid JSON fraction.");
        }
        if (position_ < text_.size() &&
            (text_[position_] == L'e' || text_[position_] == L'E'))
        {
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == L'+' || text_[position_] == L'-'))
                ++position_;
            const std::size_t exponentStart = position_;
            while (position_ < text_.size() &&
                   text_[position_] >= L'0' && text_[position_] <= L'9')
                ++position_;
            if (position_ == exponentStart)
                throw std::runtime_error("Invalid JSON exponent.");
        }
        return text_.substr(start, position_ - start);
    }

    const std::wstring& text_;
    std::size_t position_ = 0;
};

int ReadInt(JsonReader& reader)
{
    const long long value = reader.ReadInteger();
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max())
    {
        throw std::runtime_error("JSON integer is out of range.");
    }
    return static_cast<int>(value);
}

Track ReadTrack(JsonReader& reader)
{
    Track track{};
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"title") { track.title = reader.ReadString(); fields |= 1U << 0; }
        else if (name == L"artist") { track.artist = reader.ReadString(); fields |= 1U << 1; }
        else if (name == L"album") { track.album = reader.ReadString(); fields |= 1U << 2; }
        else if (name == L"comment") { track.comment = reader.ReadString(); }
        else if (name == L"duration") { track.duration = reader.ReadString(); fields |= 1U << 3; }
        else if (name == L"durationSeconds") { track.durationSeconds = ReadInt(reader); fields |= 1U << 4; }
        else if (name == L"path") { track.path = reader.ReadString(); fields |= 1U << 5; }
        else if (name == L"extinfText") { track.extinfText = reader.ReadString(); fields |= 1U << 6; }
        else if (name == L"extinfDuration") { track.extinfDuration = ReadInt(reader); fields |= 1U << 7; }
        else if (name == L"trackNumber") { track.trackNumber = reader.ReadString(); }
        else if (name == L"year") { track.year = reader.ReadString(); }
        else if (name == L"genre") { track.genre = reader.ReadString(); }
        else if (name == L"albumArtist") { track.albumArtist = reader.ReadString(); }
        else if (name == L"discNumber") { track.discNumber = reader.ReadString(); }
        else if (name == L"format") { track.format = reader.ReadString(); }
        else if (name == L"bitrate") { track.bitrate = ReadInt(reader); }
        else if (name == L"sampleRate") { track.sampleRate = ReadInt(reader); }
        else if (name == L"fileSize")
        {
            const long long value = reader.ReadInteger();
            if (value < 0)
                throw std::runtime_error("Invalid file size.");
            track.fileSize = static_cast<std::uint64_t>(value);
        }
        else if (name == L"hasFileSize") { track.hasFileSize = reader.ReadBoolean(); }
        else if (name == L"dateModified") { track.dateModified = reader.ReadString(); }
        else reader.SkipValue();
    });
    if (fields != 0xFFU)
    {
        throw std::runtime_error("Track state is incomplete.");
    }
    return track;
}

bool ReadTrackColumn(JsonReader& reader, TrackColumnConfig& column)
{
    std::wstring idName;
    int width = 100;
    bool visible = false;
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"id") { idName = reader.ReadString(); fields |= 1U; }
        else if (name == L"visible") { visible = reader.ReadBoolean(); fields |= 2U; }
        else if (name == L"width") { width = ReadInt(reader); fields |= 4U; }
        else reader.SkipValue();
    });
    TrackColumnId id{};
    if (fields != 7U || !TryParseTrackColumnId(idName, id))
    {
        return false;
    }
    const auto defaults = MakeDefaultTrackColumnConfigs();
    const auto found = std::find_if(defaults.begin(), defaults.end(),
        [id](const TrackColumnConfig& item) { return item.id == id; });
    column = found == defaults.end() ? TrackColumnConfig{id, false, 100}
                                     : *found;
    column.visible = visible;
    if (width >= 24 && width <= 4096)
    {
        column.width = width;
    }
    return true;
}

void NormalizeTrackColumns(std::vector<TrackColumnConfig>& columns)
{
    std::vector<TrackColumnConfig> normalized;
    for (const TrackColumnConfig& column : columns)
    {
        TrackColumnId parsedId{};
        const bool known = TryParseTrackColumnId(
            GetTrackColumnIdName(column.id), parsedId) &&
            parsedId == column.id;
        const bool duplicate = std::any_of(
            normalized.begin(), normalized.end(),
            [&column](const TrackColumnConfig& item) {
                return item.id == column.id;
            });
        if (known && !duplicate)
        {
            normalized.push_back(column);
        }
    }
    for (const TrackColumnConfig& defaultColumn :
         MakeDefaultTrackColumnConfigs())
    {
        if (std::none_of(normalized.begin(), normalized.end(),
            [&defaultColumn](const TrackColumnConfig& item) {
                return item.id == defaultColumn.id;
            }))
        {
            normalized.push_back(defaultColumn);
        }
    }
    for (TrackColumnConfig& column : normalized)
    {
        if (column.id == TrackColumnId::Title)
        {
            column.visible = true;
        }
    }
    columns = std::move(normalized);
}

Playlist ReadPlaylist(JsonReader& reader)
{
    Playlist playlist{};
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"name") { playlist.name = reader.ReadString(); fields |= 1U << 0; }
        else if (name == L"filePath") { playlist.filePath = reader.ReadString(); fields |= 1U << 1; }
        else if (name == L"groupId") { playlist.groupId = ReadInt(reader); }
        else if (name == L"isModified") { playlist.isModified = reader.ReadBoolean(); }
        else if (name == L"tracks")
        {
            reader.ReadArray([&]() { playlist.tracks.push_back(ReadTrack(reader)); });
            fields |= 1U << 2;
        }
        else reader.SkipValue();
    });
    if (fields != 0x07U)
    {
        throw std::runtime_error("Playlist state is incomplete.");
    }
    return playlist;
}

PlaylistGroup ReadPlaylistGroup(JsonReader& reader)
{
    PlaylistGroup group{};
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"id") { group.id = ReadInt(reader); fields |= 1U << 0; }
        else if (name == L"name") { group.name = reader.ReadString(); fields |= 1U << 1; }
        else if (name == L"expanded") { group.expanded = reader.ReadBoolean(); fields |= 1U << 2; }
        else reader.SkipValue();
    });
    if (fields != 0x07U)
    {
        throw std::runtime_error("Playlist group state is incomplete.");
    }
    return group;
}

bool ReadSendToApplication(JsonReader& reader,
                           SendToApplication& application)
{
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"name")
        {
            application.name = reader.ReadString();
            fields |= 1U << 0;
        }
        else if (name == L"executablePath")
        {
            application.executablePath = reader.ReadString();
            fields |= 1U << 1;
        }
        else if (name == L"arguments")
        {
            application.arguments = reader.ReadString();
            fields |= 1U << 2;
        }
        else reader.SkipValue();
    });
    return fields == 0x07U;
}

void ReadGui(JsonReader& reader, AppState& state)
{
    unsigned int fields = 0;
    bool readNewColumns = false;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"splitterX") { state.splitterX = ReadInt(reader); fields |= 1U << 0; }
        else if (name == L"windowWidth") { state.windowWidth = ReadInt(reader); fields |= 1U << 1; }
        else if (name == L"windowHeight") { state.windowHeight = ReadInt(reader); fields |= 1U << 2; }
        else if (name == L"windowX") { state.windowX = ReadInt(reader); fields |= 1U << 3; }
        else if (name == L"windowY") { state.windowY = ReadInt(reader); fields |= 1U << 4; }
        else if (name == L"trackColumnWidths")
        {
            std::vector<int> savedWidths;
            reader.ReadArray([&]() {
                savedWidths.push_back(ReadInt(reader));
            });
            if (!readNewColumns && (savedWidths.size() == 5 ||
                                    savedWidths.size() == 6))
            {
                const TrackColumnId legacyIds[] = {
                    TrackColumnId::Title, TrackColumnId::Artist,
                    TrackColumnId::Album, TrackColumnId::Comment,
                    TrackColumnId::Duration, TrackColumnId::Path
                };
                const TrackColumnId fiveIds[] = {
                    TrackColumnId::Title, TrackColumnId::Artist,
                    TrackColumnId::Album, TrackColumnId::Duration,
                    TrackColumnId::Path
                };
                auto& columns = state.trackColumns;
                for (std::size_t index = 0; index < savedWidths.size(); ++index)
                {
                    const TrackColumnId id = savedWidths.size() == 6
                        ? legacyIds[index] : fiveIds[index];
                    const auto found = std::find_if(
                        columns.begin(), columns.end(),
                        [id](const TrackColumnConfig& item) {
                            return item.id == id;
                        });
                    if (found != columns.end() && savedWidths[index] >= 24 &&
                        savedWidths[index] <= 4096)
                    {
                        found->width = savedWidths[index];
                    }
                }
            }
        }
        else if (name == L"trackColumns")
        {
            std::vector<TrackColumnConfig> columns;
            reader.ReadArray([&]() {
                TrackColumnConfig column{};
                if (ReadTrackColumn(reader, column))
                    columns.push_back(column);
            });
            state.trackColumns = std::move(columns);
            NormalizeTrackColumns(state.trackColumns);
            readNewColumns = true;
        }
        else reader.SkipValue();
    });
    if ((fields & 0x07U) != 0x07U)
    {
        throw std::runtime_error("GUI state is incomplete.");
    }
    state.hasWindowPosition = (fields & 0x18U) == 0x18U;
    NormalizeTrackColumns(state.trackColumns);
}

AppState DeserializeState(const std::wstring& text)
{
    JsonReader reader(text);
    AppState state{};
    state.playlistGroups.clear();
    unsigned int fields = 0;
    reader.ReadObject([&](const std::wstring& name) {
        if (name == L"version") { state.version = ReadInt(reader); fields |= 1U << 0; }
        else if (name == L"selectedPlaylistIndex") { state.selectedPlaylistIndex = ReadInt(reader); fields |= 1U << 1; }
        else if (name == L"gui") { ReadGui(reader, state); fields |= 1U << 2; }
        else if (name == L"groups")
        {
            reader.ReadArray([&]() {
                state.playlistGroups.push_back(ReadPlaylistGroup(reader));
            });
            fields |= 1U << 4;
        }
        else if (name == L"sendToApplications")
        {
            reader.ReadArray([&]() {
                SendToApplication application{};
                if (ReadSendToApplication(reader, application) &&
                    state.sendToApplications.size() <
                        MaximumSendToApplications)
                {
                    state.sendToApplications.push_back(
                        std::move(application));
                }
            });
        }
        else if (name == L"playlists")
        {
            reader.ReadArray([&]() { state.playlists.push_back(ReadPlaylist(reader)); });
            fields |= 1U << 3;
        }
        else reader.SkipValue();
    });
    reader.EnsureEnd();
    const bool hasLegacyFields = (fields & 0x0FU) == 0x0FU;
    const bool supportedVersion = state.version == 1 || state.version == 2;
    const bool hasVersionTwoGroups =
        state.version != 2 || (fields & (1U << 4)) != 0;
    if (!hasLegacyFields || !supportedVersion || !hasVersionTwoGroups)
    {
        throw std::runtime_error("Unsupported or incomplete state file.");
    }

    int nextGroupId = 1;
    NormalizePlaylistGroups(state.playlistGroups, state.playlists,
                            nextGroupId);
    state.version = 2;

    state.windowWidth = std::clamp(state.windowWidth, 360, 16384);
    state.windowHeight = std::clamp(state.windowHeight, 240, 16384);
    if (state.playlists.empty())
    {
        state.selectedPlaylistIndex = -1;
    }
    else if (state.selectedPlaylistIndex < 0 ||
             state.selectedPlaylistIndex >=
                 static_cast<int>(state.playlists.size()))
    {
        state.selectedPlaylistIndex = 0;
    }
    return state;
}
}

std::filesystem::path GetAppStateFilePath()
{
    wchar_t localAppData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA,
                                nullptr, SHGFP_TYPE_CURRENT,
                                localAppData)))
    {
        return {};
    }
    return std::filesystem::path(localAppData) /
           L"PlaylistManager" / L"state.json";
}

bool SaveAppStateToFile(const AppState& state,
                        const std::filesystem::path& filePath)
{
    try
    {
        std::error_code error;
        std::filesystem::create_directories(filePath.parent_path(), error);
        if (error)
        {
            return false;
        }

        const std::string bytes = EncodeUtf8(SerializeState(state));
        std::filesystem::path temporaryPath = filePath;
        temporaryPath += L".tmp";
        {
            std::ofstream file(temporaryPath,
                               std::ios::binary | std::ios::trunc);
            if (!file)
            {
                return false;
            }
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            file.flush();
            if (!file)
            {
                return false;
            }
        }

        if (!MoveFileExW(temporaryPath.c_str(), filePath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(temporaryPath, error);
            return false;
        }
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

AppStateLoadResult LoadAppStateFromFile(const std::filesystem::path& filePath,
                                        AppState& state)
{
    try
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(filePath, error);
        if (error)
        {
            return AppStateLoadResult::Failed;
        }
        if (!exists)
        {
            return AppStateLoadResult::NotFound;
        }
        const std::uintmax_t size = std::filesystem::file_size(filePath, error);
        if (error || size > MaximumStateFileSize)
        {
            return AppStateLoadResult::Failed;
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            return AppStateLoadResult::Failed;
        }
        std::string bytes((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        if (file.bad())
        {
            return AppStateLoadResult::Failed;
        }

        AppState loaded = DeserializeState(DecodeUtf8(std::move(bytes)));
        state = std::move(loaded);
        return AppStateLoadResult::Loaded;
    }
    catch (const std::exception&)
    {
        return AppStateLoadResult::Failed;
    }
}

bool SaveAppState(const AppState& state)
{
    const std::filesystem::path filePath = GetAppStateFilePath();
    return !filePath.empty() && SaveAppStateToFile(state, filePath);
}

AppStateLoadResult LoadAppState(AppState& state)
{
    const std::filesystem::path filePath = GetAppStateFilePath();
    return filePath.empty()
        ? AppStateLoadResult::Failed
        : LoadAppStateFromFile(filePath, state);
}
