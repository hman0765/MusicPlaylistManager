#pragma once

#include <filesystem>
#include <vector>

#include "playlist.h"

inline constexpr std::size_t MaximumSendToApplications = 10;

enum class TrackColumnId
{
    Title,
    Artist,
    Album,
    Duration,
    Comment,
    Path,
    TrackNumber,
    Year,
    Genre,
    AlbumArtist,
    DiscNumber,
    Format,
    Bitrate,
    SampleRate,
    FileSize,
    DateModified
};

struct TrackColumnConfig
{
    TrackColumnId id = TrackColumnId::Title;
    bool visible = false;
    int width = 100;
};

std::vector<TrackColumnConfig> MakeDefaultTrackColumnConfigs();
const wchar_t* GetTrackColumnIdName(TrackColumnId id);
bool TryParseTrackColumnId(const std::wstring& name, TrackColumnId& id);

struct SendToApplication
{
    std::wstring name;
    std::wstring executablePath;
    std::wstring arguments;
};

struct AppState
{
    int version = 2;
    std::vector<PlaylistGroup> playlistGroups{
        {NewPlaylistGroupId, L"New", true}
    };
    std::vector<Playlist> playlists;
    std::vector<SendToApplication> sendToApplications;
    int selectedPlaylistIndex = -1;
    int splitterX = 240;
    int windowX = 0;
    int windowY = 0;
    bool hasWindowPosition = false;
    int windowWidth = 900;
    int windowHeight = 600;
    std::vector<TrackColumnConfig> trackColumns =
        MakeDefaultTrackColumnConfigs();
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
