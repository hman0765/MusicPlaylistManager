#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include "playlist.h"

inline constexpr std::size_t TrackColumnCount = 6;
inline constexpr std::array<int, TrackColumnCount> DefaultTrackColumnWidths = {
    180, 140, 160, 180, 85, 240
};

struct AppState
{
    int version = 1;
    std::vector<Playlist> playlists;
    int selectedPlaylistIndex = -1;
    int splitterX = 240;
    int windowX = 0;
    int windowY = 0;
    bool hasWindowPosition = false;
    int windowWidth = 900;
    int windowHeight = 600;
    std::array<int, TrackColumnCount> trackColumnWidths =
        DefaultTrackColumnWidths;
};

enum class AppStateLoadResult
{
    Loaded,
    NotFound,
    Failed
};

std::filesystem::path GetAppStateFilePath();
bool SaveAppState(const AppState& state);
AppStateLoadResult LoadAppState(AppState& state);

// File-specific variants keep serialization reusable for tests and backups.
bool SaveAppStateToFile(const AppState& state,
                        const std::filesystem::path& filePath);
AppStateLoadResult LoadAppStateFromFile(const std::filesystem::path& filePath,
                                        AppState& state);
