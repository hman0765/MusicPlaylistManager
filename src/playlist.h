#pragma once

#include <string>
#include <vector>

struct Track
{
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring duration;
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
Playlist LoadM3U8(const std::wstring& filePath);
std::wstring FormatDuration(int seconds);
void AddTrack(Playlist& playlist, Track track);
