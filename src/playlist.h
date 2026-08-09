#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

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
    std::wstring duration;
    int durationSeconds = -1;
    std::wstring path;
    std::wstring extinfText;
    int extinfDuration = -1;
};

struct Playlist
{
    std::wstring name;
    std::wstring filePath;
    std::vector<Track> tracks;
    bool isModified = false;
};

bool IsSupportedAudioPath(const std::wstring& path);
bool IsM3U8Path(const std::wstring& path);
Track CreateTrackFromFile(const std::wstring& path);
bool UpdateTrackMetadata(Track& track);
Playlist LoadM3U8(const std::wstring& filePath);
void SaveM3U8(const Playlist& playlist, const std::wstring& filePath);
std::wstring BuildExtinfText(const Track& track);
int GetExportDuration(const Track& track);
std::wstring FormatDuration(int seconds);
void AddTrack(Playlist& playlist, Track track);
