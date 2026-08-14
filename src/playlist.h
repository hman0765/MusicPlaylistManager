#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

inline constexpr int NewPlaylistGroupId = 0;

struct PlaylistGroup
{
    int id = -1;
    std::wstring name;
    bool expanded = true;
};

template <typename T>
std::size_t MoveVectorItem(std::vector<T>& items,
                           std::size_t fromIndex,
                           std::size_t insertionIndex)
{
    if (fromIndex >= items.size())
    {
        return fromIndex;
    }

    insertionIndex = std::min(insertionIndex, items.size());
    if (insertionIndex == fromIndex || insertionIndex == fromIndex + 1)
    {
        return fromIndex;
    }

    T movedItem = std::move(items[fromIndex]);
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(fromIndex));
    if (insertionIndex > fromIndex)
    {
        --insertionIndex;
    }
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
                 std::move(movedItem));
    return insertionIndex;
}

inline int RemapIndexAfterMove(int index, int fromIndex, int toIndex)
{
    if (index == fromIndex)
    {
        return toIndex;
    }
    if (fromIndex < toIndex && index > fromIndex && index <= toIndex)
    {
        return index - 1;
    }
    if (toIndex < fromIndex && index >= toIndex && index < fromIndex)
    {
        return index + 1;
    }
    return index;
}

struct Track
{
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring comment;
    std::wstring duration;
    int durationSeconds = -1;
    std::wstring path;
    std::wstring extinfText;
    int extinfDuration = -1;
    std::wstring trackNumber;
    std::wstring year;
    std::wstring genre;
    std::wstring albumArtist;
    std::wstring discNumber;
    std::wstring format;
    int bitrate = -1;
    int sampleRate = -1;
    std::uint64_t fileSize = 0;
    bool hasFileSize = false;
    std::wstring dateModified;
};

struct MetadataRequest
{
    bool title = false;
    bool artist = false;
    bool album = false;
    bool comment = false;
    bool trackNumber = false;
    bool year = false;
    bool genre = false;
    bool albumArtist = false;
    bool discNumber = false;
    bool duration = false;
    bool format = false;
    bool bitrate = false;
    bool sampleRate = false;
    bool fileSize = false;
    bool dateModified = false;
};

struct Playlist
{
    std::wstring name;
    std::wstring filePath;
    int groupId = NewPlaylistGroupId;
    std::vector<Track> tracks;
    bool isModified = false;
};

bool IsSupportedAudioPath(const std::wstring& path);
bool IsM3U8Path(const std::wstring& path);
Track CreateTrackFromFile(const std::wstring& path);
bool UpdateTrackMetadata(Track& track);
bool UpdateTrackMetadata(Track& track, const MetadataRequest& request);
Playlist LoadM3U8(const std::wstring& filePath);
void SaveM3U8(const Playlist& playlist, const std::wstring& filePath);
std::wstring BuildExtinfText(const Track& track);
int GetExportDuration(const Track& track);
std::wstring FormatDuration(int seconds);
void AddTrack(Playlist& playlist, Track track);

PlaylistGroup* FindPlaylistGroupById(
    std::vector<PlaylistGroup>& groups, int groupId);
const PlaylistGroup* FindPlaylistGroupById(
    const std::vector<PlaylistGroup>& groups, int groupId);
bool IsPlaylistGroupNameAvailable(
    const std::vector<PlaylistGroup>& groups, const std::wstring& name);
std::wstring GenerateNewGroupName(
    const std::vector<PlaylistGroup>& groups);
int CreatePlaylistGroup(std::vector<PlaylistGroup>& groups,
                        int& nextGroupId, const std::wstring& name);
void NormalizePlaylistGroups(std::vector<PlaylistGroup>& groups,
                             std::vector<Playlist>& playlists,
                             int& nextGroupId);
