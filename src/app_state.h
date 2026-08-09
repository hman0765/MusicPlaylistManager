#pragma once

#include <filesystem>
#include <vector>

#include "playlist.h"

struct AppState
{
    int version = 1;
    std::vector<Playlist> playlists;
    int selectedPlaylistIndex = -1;
    int splitterX = 240;
    int windowWidth = 900;
    int windowHeight = 600;
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
