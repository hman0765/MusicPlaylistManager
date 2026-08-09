#pragma once

#include <string>
#include <vector>

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
Playlist LoadM3U8(const std::wstring& filePath);
void SaveM3U8(const Playlist& playlist, const std::wstring& filePath);
std::wstring BuildExtinfText(const Track& track);
int GetExportDuration(const Track& track);
std::wstring FormatDuration(int seconds);
void AddTrack(Playlist& playlist, Track track);
