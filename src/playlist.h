#pragma once

#include <string>
#include <vector>

struct Track
{
    std::wstring path;
    std::wstring title;
};

struct Playlist
{
    std::wstring name;
    std::wstring filePath;
    std::vector<Track> tracks;
    bool isModified = false;
};

bool IsSupportedAudioPath(const std::wstring& path);
void AddTrack(Playlist& playlist, const std::wstring& path);
