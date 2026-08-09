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
};

struct Playlist
{
    std::wstring name;
    std::wstring filePath;
    std::vector<Track> tracks;
    bool isModified = false;
};

bool IsSupportedAudioPath(const std::wstring& path);
Track CreateTrackFromFile(const std::wstring& path);
void AddTrack(Playlist& playlist, Track track);
