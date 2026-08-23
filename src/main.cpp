#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "app_state.h"
#include "drag_drop.h"
#include "playlist.h"
#include "resource.h"
#include "version.h"

namespace
{
constexpr wchar_t WindowClassName[] = L"MusicPlaylistManagerWindow";
constexpr wchar_t OrganizerWindowClassName[] =
    L"MusicPlaylistManagerOrganizerWindow";
constexpr wchar_t SendToSettingsWindowClassName[] =
    L"MusicPlaylistManagerSendToSettingsWindow";
constexpr wchar_t SendToEditorWindowClassName[] =
    L"MusicPlaylistManagerSendToEditorWindow";
constexpr wchar_t TrackColumnsWindowClassName[] =
    L"MusicPlaylistManagerTrackColumnsWindow";
constexpr wchar_t ExtinfFormatWindowClassName[] =
    L"MusicPlaylistManagerExtinfFormatWindow";
constexpr wchar_t CustomExtinfEditorWindowClassName[] =
    L"MusicPlaylistManagerCustomExtinfEditorWindow";
constexpr wchar_t WindowTitle[] = L"Music Playlist Manager";
constexpr wchar_t OnlineManualUrl[] =
    L"https://app2026kak.netlify.app/playlist-manager/manual";
constexpr int SplitterWidth = 6;
constexpr int MinimumPaneWidth = 120;
constexpr int MinimumWindowWidth = 360;
constexpr int MinimumWindowHeight = 240;
constexpr UINT CommandNewPlaylist = 1001;
constexpr UINT CommandRenamePlaylist = 1002;
constexpr UINT CommandDeletePlaylist = 1003;
constexpr UINT CommandExportM3U8 = 1004;
constexpr UINT CommandGetTrackMetadata = 1005;
constexpr UINT CommandDeleteTracks = 1006;
constexpr UINT CommandOpenTracksInExplorer = 1007;
constexpr UINT CommandShowTrackProperties = 1008;
constexpr UINT CommandOpenAudioFiles = 1009;
constexpr UINT CommandImportPlaylist = 1010;
constexpr UINT CommandExit = 1011;
constexpr UINT CommandOrganizePlaylists = 1012;
constexpr UINT CommandSendToApplications = 1013;
constexpr UINT CommandTrackColumns = 1014;
constexpr UINT CommandExtinfFormat = 1015;
constexpr UINT CommandAbout = 1016;
constexpr UINT CommandLanguageEnglish = 1017;
constexpr UINT CommandLanguageJapanese = 1018;
constexpr UINT CommandOnlineManual = 1019;
constexpr UINT SendToApplicationCommandBase = 12000;
constexpr UINT MaximumWindowsCommandLineLength = 32767;
constexpr UINT MessageRefreshPlaylistList = WM_APP + 1;
constexpr UINT MessagePreparePlaylistLabelEdit = WM_APP + 2;
constexpr UINT MessageTogglePlaylistGroup = WM_APP + 3;
constexpr UINT_PTR AppStateTimerId = 1;
constexpr UINT AppStateTimerIntervalMs = 60'000;
constexpr int OrganizerGroupListId = 2001;
constexpr int OrganizerPlaylistListId = 2002;
constexpr int OrganizerNewGroupButtonId = 2003;
constexpr int OrganizerCloseButtonId = 2004;
constexpr UINT OrganizerCommandNewGroup = 2101;
constexpr UINT OrganizerCommandRenameGroup = 2102;
constexpr UINT OrganizerCommandDeleteGroup = 2103;
constexpr UINT OrganizerCommandDeletePlaylists = 2104;
constexpr UINT OrganizerMoveToGroupCommandBase = 22000;

enum class SendToArgumentMode
{
    Invalid,
    Files,
    Folder
};
constexpr int SendToListId = 3001;
constexpr int SendToAddButtonId = 3002;
constexpr int SendToEditButtonId = 3003;
constexpr int SendToRemoveButtonId = 3004;
constexpr int SendToCloseButtonId = 3005;
constexpr int SendToMoveUpButtonId = 3006;
constexpr int SendToMoveDownButtonId = 3007;
constexpr int SendToEditorNameId = 3101;
constexpr int SendToEditorExecutableId = 3102;
constexpr int SendToEditorBrowseId = 3103;
constexpr int SendToEditorArgumentsId = 3104;
constexpr int SendToEditorOkId = 3105;
constexpr int SendToEditorCancelId = 3106;
constexpr int TrackColumnsListId = 4001;
constexpr int TrackColumnsUpButtonId = 4002;
constexpr int TrackColumnsDownButtonId = 4003;
constexpr int TrackColumnsCloseButtonId = 4004;
constexpr int ExtinfArtistTitleRadioId = 5001;
constexpr int ExtinfTitleRadioId = 5002;
constexpr int ExtinfArtistTitleAlbumRadioId = 5003;
constexpr int ExtinfCustomRadioId = 5004;
constexpr int ExtinfCustomTextId = 5005;
constexpr int ExtinfEditButtonId = 5006;
constexpr int ExtinfPreviewTextId = 5007;
constexpr int ExtinfCloseButtonId = 5008;
constexpr int CustomExtinfEditId = 5101;
constexpr int CustomExtinfOkId = 5102;
constexpr int CustomExtinfCancelId = 5103;

HWND mainWindow = nullptr;
HWND playlistListView = nullptr;
HWND trackListView = nullptr;
HWND statusText = nullptr;
HWND playlistTooltip = nullptr;
HWND trackTooltip = nullptr;
HMENU fileMenu = nullptr;
HMENU playlistMenu = nullptr;
HMENU trackMenu = nullptr;
HMENU trackSendToMenu = nullptr;
HMENU settingsMenu = nullptr;
HMENU helpMenu = nullptr;
HMENU languageMenu = nullptr;
HWND organizerWindow = nullptr;
HWND organizerGroupList = nullptr;
HWND organizerPlaylistList = nullptr;
HWND organizerNewGroupButton = nullptr;
HWND organizerCloseButton = nullptr;
int selectedOrganizerGroupIndex = 0;
int organizerContextGroupId = -1;
bool isRefreshingOrganizerGroups = false;
std::vector<int> organizerVisiblePlaylistIndices;
std::vector<int> organizerMoveMenuGroupIds;
HWND sendToSettingsWindow = nullptr;
HWND sendToList = nullptr;
HWND sendToAddButton = nullptr;
HWND sendToEditButton = nullptr;
HWND sendToRemoveButton = nullptr;
HWND sendToCloseButton = nullptr;
HWND sendToMoveUpButton = nullptr;
HWND sendToMoveDownButton = nullptr;
HWND sendToEditorWindow = nullptr;
HWND sendToEditorName = nullptr;
HWND sendToEditorExecutable = nullptr;
HWND sendToEditorArguments = nullptr;
int sendToEditorIndex = -1;
HWND trackColumnsWindow = nullptr;
HWND trackColumnsList = nullptr;
HWND trackColumnsUpButton = nullptr;
HWND trackColumnsDownButton = nullptr;
HWND trackColumnsCloseButton = nullptr;
bool isRefreshingTrackColumns = false;
HWND extinfFormatWindow = nullptr;
HWND extinfCustomText = nullptr;
HWND extinfEditButton = nullptr;
HWND extinfPreviewText = nullptr;
HWND customExtinfEditorWindow = nullptr;
HWND customExtinfEdit = nullptr;
HFONT statusFont = nullptr;
int statusHeight = 32;
int splitterX = 240;
int savedWindowX = 0;
int savedWindowY = 0;
bool hasSavedWindowPosition = false;
int savedWindowWidth = 900;
int savedWindowHeight = 600;
std::vector<TrackColumnConfig> trackColumnConfigs =
    MakeDefaultTrackColumnConfigs();
std::vector<TrackColumnId> visibleTrackColumnIds;
ExtinfFormatPreset extinfFormatPreset = ExtinfFormatPreset::ArtistTitle;
std::wstring customExtinfFormat = DefaultCustomExtinfFormat;
AppLanguage activeLanguage = AppLanguage::English;
AppLanguage languageSetting = AppLanguage::English;
bool isDraggingSplitter = false;
int splitterDragOffset = 0;
int splitterXAtDragStart = 240;
bool oleDragDropAvailable = false;

std::vector<PlaylistGroup> playlistGroups{
    {NewPlaylistGroupId, L"New", true}
};
int nextGroupId = 1;
std::vector<Playlist> playlists{
    {L"New Playlist", L"", NewPlaylistGroupId, {}, false}
};
std::vector<SendToApplication> sendToApplications;
int selectedPlaylistIndex = 0;
bool isRefreshingPlaylistList = false;
bool isRefreshingTrackList = false;
bool appStateDirty = false;
bool appStateTrackingEnabled = false;

const wchar_t* T(UiText id)
{
    return GetUiText(id, activeLanguage);
}

void OpenOnlineManual(HWND owner)
{
    const HINSTANCE result = ShellExecuteW(
        owner, L"open", OnlineManualUrl, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxW(owner, T(UiText::OnlineManualOpenFailed), WindowTitle,
                    MB_OK | MB_ICONERROR);
    }
}

enum class PlaylistListRowType
{
    Group,
    Playlist
};

struct PlaylistListRow
{
    PlaylistListRowType type = PlaylistListRowType::Group;
    int groupIndex = -1;
    int playlistIndex = -1;
};

std::vector<PlaylistListRow> visiblePlaylistRows;

struct ListTooltipState
{
    HWND listView = nullptr;
    HWND tooltip = nullptr;
    int row = -1;
    int column = -1;
    std::wstring text;
};

ListTooltipState playlistTooltipState{};
ListTooltipState trackTooltipState{};
constexpr UINT_PTR PlaylistTooltipSubclassId = 1;
constexpr UINT_PTR TrackTooltipSubclassId = 2;

void ResetListTooltip(ListTooltipState& state);
SendToArgumentMode GetSendToArgumentMode(const std::wstring& arguments);
bool IsValidSendToArguments(const std::wstring& arguments);
bool CanUseSendToApplication(const SendToApplication& application,
                             int selectedTrackCount);
void ShowPlaylistOrganizer(HWND owner);
void ShowSendToApplications(HWND owner);
void ShowTrackColumns(HWND owner);
void ShowExtinfFormatSettings(HWND owner);
void SaveCurrentTrackColumnWidths();
void RebuildTrackListColumns();
LRESULT CALLBACK PlaylistOrganizerWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SendToSettingsWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SendToEditorWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TrackColumnsWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ExtinfFormatWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CustomExtinfEditorWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

void MarkAppStateDirty()
{
    if (appStateTrackingEnabled)
    {
        appStateDirty = true;
    }
}

AppState CaptureCurrentAppState()
{
    SaveCurrentTrackColumnWidths();
    AppState state{};
    state.playlistGroups = playlistGroups;
    state.playlists = playlists;
    state.sendToApplications = sendToApplications;
    state.selectedPlaylistIndex = selectedPlaylistIndex;
    state.splitterX = splitterX;
    RECT windowRect{};
    if (mainWindow != nullptr && !IsIconic(mainWindow) &&
        GetWindowRect(mainWindow, &windowRect))
    {
        state.windowX = windowRect.left;
        state.windowY = windowRect.top;
        state.hasWindowPosition = true;
        state.windowWidth = windowRect.right - windowRect.left;
        state.windowHeight = windowRect.bottom - windowRect.top;
    }
    else
    {
        state.windowX = savedWindowX;
        state.windowY = savedWindowY;
        state.hasWindowPosition = hasSavedWindowPosition;
        state.windowWidth = savedWindowWidth;
        state.windowHeight = savedWindowHeight;
    }
    state.trackColumns = trackColumnConfigs;
    state.extinfFormatPreset = extinfFormatPreset;
    state.customExtinfFormat = customExtinfFormat;
    state.language = languageSetting;
    return state;
}

bool SaveCurrentAppState()
{
    if (!appStateDirty)
    {
        return true;
    }
    if (!SaveAppState(CaptureCurrentAppState()))
    {
        return false;
    }
    appStateDirty = false;
    return true;
}

void ApplyLoadedAppState(AppState state)
{
    playlistGroups = std::move(state.playlistGroups);
    playlists = std::move(state.playlists);
    sendToApplications = std::move(state.sendToApplications);
    NormalizePlaylistGroups(playlistGroups, playlists, nextGroupId);
    selectedPlaylistIndex = state.selectedPlaylistIndex;
    splitterX = state.splitterX;
    savedWindowX = state.windowX;
    savedWindowY = state.windowY;
    hasSavedWindowPosition = state.hasWindowPosition;
    savedWindowWidth = state.windowWidth;
    savedWindowHeight = state.windowHeight;
    trackColumnConfigs = std::move(state.trackColumns);
    extinfFormatPreset = state.extinfFormatPreset;
    customExtinfFormat = std::move(state.customExtinfFormat);
    activeLanguage = state.language;
    languageSetting = state.language;
}

void ResetToDefaultAppState()
{
    playlistGroups = {{NewPlaylistGroupId, L"New", true}};
    nextGroupId = 1;
    playlists = {{L"New Playlist", L"", NewPlaylistGroupId, {}, false}};
    sendToApplications.clear();
    selectedPlaylistIndex = 0;
    splitterX = 240;
    savedWindowX = 0;
    savedWindowY = 0;
    hasSavedWindowPosition = false;
    savedWindowWidth = 900;
    savedWindowHeight = 600;
    trackColumnConfigs = MakeDefaultTrackColumnConfigs();
    extinfFormatPreset = ExtinfFormatPreset::ArtistTitle;
    customExtinfFormat = DefaultCustomExtinfFormat;
    activeLanguage = AppLanguage::English;
    languageSetting = AppLanguage::English;
    appStateDirty = false;
}

void KeepSavedWindowPositionOnScreen()
{
    if (!hasSavedWindowPosition)
    {
        return;
    }

    const auto addWithoutOverflow = [](int position, int size) {
        const long long result = static_cast<long long>(position) + size;
        return static_cast<LONG>(std::clamp(
            result,
            static_cast<long long>(std::numeric_limits<LONG>::min()),
            static_cast<long long>(std::numeric_limits<LONG>::max())));
    };
    RECT savedRect{savedWindowX, savedWindowY,
                   addWithoutOverflow(savedWindowX, savedWindowWidth),
                   addWithoutOverflow(savedWindowY, savedWindowHeight)};
    const HMONITOR monitor = MonitorFromRect(
        &savedRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        hasSavedWindowPosition = false;
        return;
    }

    // Keep part of the title bar reachable even after the monitor layout changes.
    constexpr int MinimumVisibleTitleWidth = 120;
    constexpr int VisibleTitleHeight = 32;
    const int visibleWidth = std::min(savedWindowWidth,
                                      MinimumVisibleTitleWidth);
    const int previousX = savedWindowX;
    const int previousY = savedWindowY;
    savedWindowX = std::clamp(
        savedWindowX,
        static_cast<int>(monitorInfo.rcWork.left) - savedWindowWidth +
            visibleWidth,
        static_cast<int>(monitorInfo.rcWork.right) - visibleWidth);
    savedWindowY = std::clamp(
        savedWindowY, static_cast<int>(monitorInfo.rcWork.top),
        static_cast<int>(monitorInfo.rcWork.bottom) - VisibleTitleHeight);
    if (savedWindowX != previousX || savedWindowY != previousY)
    {
        appStateDirty = true;
    }
}

class OleApartment
{
public:
    OleApartment()
        : initialized(SUCCEEDED(OleInitialize(nullptr)))
    {
    }

    ~OleApartment()
    {
        if (initialized)
        {
            OleUninitialize();
        }
    }

    bool IsAvailable() const
    {
        return initialized;
    }

    OleApartment(const OleApartment&) = delete;
    OleApartment& operator=(const OleApartment&) = delete;

private:
    bool initialized;
};

void InsertColumn(HWND listView, int index, const wchar_t* heading, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    column.pszText = const_cast<wchar_t*>(heading);
    column.cx = width;
    column.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(listView, index, &column);
}

void InsertListItem(HWND listView, int index, const std::wstring& text)
{
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<wchar_t*>(text.c_str());
    ListView_InsertItem(listView, &item);
}

Playlist* GetSelectedPlaylist()
{
    if (selectedPlaylistIndex < 0 ||
        selectedPlaylistIndex >= static_cast<int>(playlists.size()))
    {
        return nullptr;
    }
    return &playlists[static_cast<std::size_t>(selectedPlaylistIndex)];
}

int GetPlaylistIndexFromVisibleRow(int row)
{
    if (row < 0 || row >= static_cast<int>(visiblePlaylistRows.size()))
    {
        return -1;
    }
    const PlaylistListRow& visibleRow =
        visiblePlaylistRows[static_cast<std::size_t>(row)];
    return visibleRow.type == PlaylistListRowType::Playlist
        ? visibleRow.playlistIndex
        : -1;
}

int GetGroupIndexFromVisibleRow(int row)
{
    if (row < 0 || row >= static_cast<int>(visiblePlaylistRows.size()))
    {
        return -1;
    }
    const PlaylistListRow& visibleRow =
        visiblePlaylistRows[static_cast<std::size_t>(row)];
    return visibleRow.type == PlaylistListRowType::Group
        ? visibleRow.groupIndex
        : -1;
}

int FindVisibleRowForPlaylist(int playlistIndex)
{
    for (std::size_t row = 0; row < visiblePlaylistRows.size(); ++row)
    {
        if (visiblePlaylistRows[row].type == PlaylistListRowType::Playlist &&
            visiblePlaylistRows[row].playlistIndex == playlistIndex)
        {
            return static_cast<int>(row);
        }
    }
    return -1;
}

std::wstring GetGroupDisplayText(const PlaylistGroup& group)
{
    return (group.expanded ? L"\u25bc " : L"\u25b6 ") + group.name;
}

std::wstring GetPlaylistDisplayText(const Playlist& playlist)
{
    return L"    " + playlist.name;
}

bool EnsurePlaylistGroupExpanded(int groupId)
{
    PlaylistGroup* group = FindPlaylistGroupById(playlistGroups, groupId);
    if (group == nullptr || group->expanded)
    {
        return false;
    }
    group->expanded = true;
    return true;
}

void RefreshPlaylistList(HWND listView)
{
    ResetListTooltip(playlistTooltipState);
    isRefreshingPlaylistList = true;
    ListView_DeleteAllItems(listView);
    visiblePlaylistRows.clear();

    for (std::size_t groupIndex = 0; groupIndex < playlistGroups.size();
         ++groupIndex)
    {
        const PlaylistGroup& group = playlistGroups[groupIndex];
        visiblePlaylistRows.push_back(
            {PlaylistListRowType::Group,
             static_cast<int>(groupIndex), -1});
        InsertListItem(listView,
                       static_cast<int>(visiblePlaylistRows.size()) - 1,
                       GetGroupDisplayText(group));
        if (!group.expanded)
        {
            continue;
        }
        for (std::size_t playlistIndex = 0;
             playlistIndex < playlists.size(); ++playlistIndex)
        {
            if (playlists[playlistIndex].groupId != group.id)
            {
                continue;
            }
            visiblePlaylistRows.push_back(
                {PlaylistListRowType::Playlist, static_cast<int>(groupIndex),
                 static_cast<int>(playlistIndex)});
            InsertListItem(listView,
                           static_cast<int>(visiblePlaylistRows.size()) - 1,
                           GetPlaylistDisplayText(playlists[playlistIndex]));
        }
    }

    if (selectedPlaylistIndex >= 0 &&
        selectedPlaylistIndex < static_cast<int>(playlists.size()))
    {
        const int selectedRow =
            FindVisibleRowForPlaylist(selectedPlaylistIndex);
        if (selectedRow >= 0)
        {
            ListView_SetItemState(listView, selectedRow,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(listView, selectedRow, FALSE);
        }
    }
    else
    {
        selectedPlaylistIndex = -1;
    }
    isRefreshingPlaylistList = false;
}

void SetListItemText(HWND listView, int row, int column,
                     const std::wstring& text)
{
    ListView_SetItemText(listView, row, column,
                         const_cast<wchar_t*>(text.c_str()));
}

void RefreshStatusBar();

std::wstring GetTrackColumnName(TrackColumnId id)
{
    switch (id)
    {
    case TrackColumnId::Title: return T(UiText::TitleColumn);
    case TrackColumnId::Artist: return T(UiText::ArtistColumn);
    case TrackColumnId::Album: return T(UiText::AlbumColumn);
    case TrackColumnId::Duration: return T(UiText::DurationColumn);
    case TrackColumnId::Comment: return T(UiText::CommentColumn);
    case TrackColumnId::Path: return T(UiText::PathColumn);
    case TrackColumnId::TrackNumber: return T(UiText::TrackNumberColumn);
    case TrackColumnId::Year: return T(UiText::YearColumn);
    case TrackColumnId::Genre: return T(UiText::GenreColumn);
    case TrackColumnId::AlbumArtist: return T(UiText::AlbumArtistColumn);
    case TrackColumnId::DiscNumber: return T(UiText::DiscNumberColumn);
    case TrackColumnId::Format: return T(UiText::FormatColumn);
    case TrackColumnId::Bitrate: return T(UiText::BitrateColumn);
    case TrackColumnId::SampleRate: return T(UiText::SampleRateColumn);
    case TrackColumnId::FileSize: return T(UiText::FileSizeColumn);
    case TrackColumnId::DateModified: return T(UiText::DateModifiedColumn);
    }
    return L"";
}

TrackColumnConfig* FindTrackColumnConfig(TrackColumnId id)
{
    const auto found = std::find_if(
        trackColumnConfigs.begin(), trackColumnConfigs.end(),
        [id](const TrackColumnConfig& column) { return column.id == id; });
    return found == trackColumnConfigs.end() ? nullptr : &*found;
}

bool IsTrackColumnVisible(TrackColumnId id)
{
    const TrackColumnConfig* column = FindTrackColumnConfig(id);
    return column != nullptr && column->visible;
}

std::wstring FormatFileSize(std::uint64_t bytes)
{
    static constexpr const wchar_t* Units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(Units))
    {
        value /= 1024.0;
        ++unit;
    }
    std::wostringstream text;
    if (unit == 0)
        text << static_cast<std::uint64_t>(value);
    else
        text << std::fixed << std::setprecision(1) << value;
    text << L' ' << Units[unit];
    return text.str();
}

std::wstring GetTrackColumnDisplayText(const Track& track, TrackColumnId id)
{
    switch (id)
    {
    case TrackColumnId::Title:
        return track.title.empty() ? track.extinfText : track.title;
    case TrackColumnId::Artist: return track.artist;
    case TrackColumnId::Album: return track.album;
    case TrackColumnId::Duration:
        return track.duration.empty()
            ? FormatDuration(track.extinfDuration)
            : track.duration;
    case TrackColumnId::Comment: return track.comment;
    case TrackColumnId::Path: return track.path;
    case TrackColumnId::TrackNumber: return track.trackNumber;
    case TrackColumnId::Year: return track.year;
    case TrackColumnId::Genre: return track.genre;
    case TrackColumnId::AlbumArtist: return track.albumArtist;
    case TrackColumnId::DiscNumber: return track.discNumber;
    case TrackColumnId::Format: return track.format;
    case TrackColumnId::Bitrate:
        return track.bitrate < 0 ? L"" :
            std::to_wstring(track.bitrate) + L" kbps";
    case TrackColumnId::SampleRate:
        return track.sampleRate < 0 ? L"" :
            std::to_wstring(track.sampleRate) + L" Hz";
    case TrackColumnId::FileSize:
        return track.hasFileSize ? FormatFileSize(track.fileSize) : L"";
    case TrackColumnId::DateModified: return track.dateModified;
    }
    return L"";
}

void RefreshTrackList(HWND listView, const Playlist& playlist)
{
    ResetListTooltip(trackTooltipState);
    isRefreshingTrackList = true;
    ListView_DeleteAllItems(listView);
    for (std::size_t index = 0; index < playlist.tracks.size(); ++index)
    {
        const int row = static_cast<int>(index);
        const Track& track = playlist.tracks[index];
        if (visibleTrackColumnIds.empty())
            continue;
        InsertListItem(listView, row, GetTrackColumnDisplayText(
            track, visibleTrackColumnIds.front()));
        for (std::size_t column = 1;
             column < visibleTrackColumnIds.size(); ++column)
        {
            SetListItemText(listView, row, static_cast<int>(column),
                GetTrackColumnDisplayText(track,
                                          visibleTrackColumnIds[column]));
        }
    }
    isRefreshingTrackList = false;
}

void RefreshSelectedTrackList()
{
    if (const Playlist* playlist = GetSelectedPlaylist())
    {
        RefreshTrackList(trackListView, *playlist);
    }
    else
    {
        ResetListTooltip(trackTooltipState);
        ListView_DeleteAllItems(trackListView);
    }
    RefreshStatusBar();
}

std::vector<int> GetSelectedTrackIndices(HWND listView)
{
    std::vector<int> indices;
    int index = -1;
    while ((index = ListView_GetNextItem(listView, index, LVNI_SELECTED)) != -1)
    {
        indices.push_back(index);
    }
    return indices;
}

std::wstring GetTooltipCellText(HWND listView, int row, int column)
{
    if (listView == playlistListView)
    {
        if (column != 0 || row < 0 ||
            row >= static_cast<int>(visiblePlaylistRows.size()))
        {
            return L"";
        }
        const PlaylistListRow& visibleRow =
            visiblePlaylistRows[static_cast<std::size_t>(row)];
        if (visibleRow.type == PlaylistListRowType::Group &&
            visibleRow.groupIndex >= 0 &&
            visibleRow.groupIndex < static_cast<int>(playlistGroups.size()))
        {
            return playlistGroups[
                static_cast<std::size_t>(visibleRow.groupIndex)].name;
        }
        if (visibleRow.type == PlaylistListRowType::Playlist &&
            visibleRow.playlistIndex >= 0 &&
            visibleRow.playlistIndex < static_cast<int>(playlists.size()))
        {
            return playlists[
                static_cast<std::size_t>(visibleRow.playlistIndex)].name;
        }
        return L"";
    }

    const Playlist* playlist = GetSelectedPlaylist();
    if (listView != trackListView || playlist == nullptr || column < 0 ||
        column >= static_cast<int>(visibleTrackColumnIds.size()) ||
        row < 0 || row >= static_cast<int>(playlist->tracks.size()))
    {
        return L"";
    }
    return GetTrackColumnDisplayText(
        playlist->tracks[static_cast<std::size_t>(row)],
        visibleTrackColumnIds[static_cast<std::size_t>(column)]);
}

std::wstring GetTooltipMeasurementText(HWND listView, int row, int column,
                                       const std::wstring& tooltipText)
{
    if (listView != playlistListView || column != 0 || row < 0 ||
        row >= static_cast<int>(visiblePlaylistRows.size()))
    {
        return tooltipText;
    }
    const PlaylistListRow& visibleRow =
        visiblePlaylistRows[static_cast<std::size_t>(row)];
    if (visibleRow.type == PlaylistListRowType::Group &&
        visibleRow.groupIndex >= 0 &&
        visibleRow.groupIndex < static_cast<int>(playlistGroups.size()))
    {
        return GetGroupDisplayText(playlistGroups[
            static_cast<std::size_t>(visibleRow.groupIndex)]);
    }
    if (visibleRow.type == PlaylistListRowType::Playlist)
    {
        return L"    " + tooltipText;
    }
    return tooltipText;
}

bool IsCellTextTruncated(HWND listView, const RECT& cellRect,
                         const std::wstring& text)
{
    if (text.empty())
    {
        return false;
    }

    HDC deviceContext = GetDC(listView);
    if (deviceContext == nullptr)
    {
        return false;
    }
    HFONT font = reinterpret_cast<HFONT>(
        SendMessageW(listView, WM_GETFONT, 0, 0));
    if (font == nullptr)
    {
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    const HGDIOBJ previousFont = SelectObject(deviceContext, font);
    SIZE textSize{};
    const bool measured = GetTextExtentPoint32W(
        deviceContext, text.c_str(), static_cast<int>(text.size()),
        &textSize) != FALSE;
    SelectObject(deviceContext, previousFont);
    ReleaseDC(listView, deviceContext);

    constexpr int HorizontalCellPadding = 12;
    const int availableWidth =
        std::max(0, static_cast<int>(cellRect.right - cellRect.left) -
                        HorizontalCellPadding);
    return measured && textSize.cx > availableWidth;
}

void ResetListTooltip(ListTooltipState& state)
{
    if (state.tooltip != nullptr)
    {
        TOOLINFOW toolInfo{};
        toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
        toolInfo.hwnd = GetParent(state.listView);
        toolInfo.uId = reinterpret_cast<UINT_PTR>(state.listView);
        SendMessageW(state.tooltip, TTM_TRACKACTIVATE, FALSE,
                     reinterpret_cast<LPARAM>(&toolInfo));
    }
    state.row = -1;
    state.column = -1;
    state.text.clear();
}

void UpdateListTooltip(ListTooltipState& state, POINT mousePosition)
{
    LVHITTESTINFO hitTest{};
    hitTest.pt = mousePosition;
    const int row = ListView_SubItemHitTest(state.listView, &hitTest);
    const int column = hitTest.iSubItem;
    if (row < 0 || column < 0)
    {
        ResetListTooltip(state);
        return;
    }

    RECT cellRect{};
    if (!ListView_GetSubItemRect(state.listView, row, column,
                                 LVIR_BOUNDS, &cellRect))
    {
        ResetListTooltip(state);
        return;
    }
    // For subitem 0, LVIR_BOUNDS can cover the complete row, including all
    // following subitems. Limit the rectangle to the actual first column.
    if (state.listView == trackListView && column == 0)
    {
        const int titleColumnWidth =
            ListView_GetColumnWidth(state.listView, 0);
        if (titleColumnWidth <= 0)
        {
            ResetListTooltip(state);
            return;
        }
        cellRect.right = cellRect.left + titleColumnWidth;
    }
    const std::wstring text = GetTooltipCellText(
        state.listView, row, column);
    const std::wstring measurementText = GetTooltipMeasurementText(
        state.listView, row, column, text);
    if (!IsCellTextTruncated(state.listView, cellRect, measurementText))
    {
        ResetListTooltip(state);
        return;
    }
    if (state.row == row && state.column == column && state.text == text)
    {
        return;
    }

    ResetListTooltip(state);
    state.row = row;
    state.column = column;
    state.text = text;
    POINT tooltipPosition = mousePosition;
    ClientToScreen(state.listView, &tooltipPosition);
    SendMessageW(state.tooltip, TTM_TRACKPOSITION, 0,
                 MAKELPARAM(tooltipPosition.x + 16,
                            tooltipPosition.y + 20));
    TOOLINFOW toolInfo{};
    toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
    toolInfo.hwnd = GetParent(state.listView);
    toolInfo.uId = reinterpret_cast<UINT_PTR>(state.listView);
    toolInfo.lpszText = state.text.data();
    SendMessageW(state.tooltip, TTM_UPDATETIPTEXTW, 0,
                 reinterpret_cast<LPARAM>(&toolInfo));
    SendMessageW(state.tooltip, TTM_TRACKACTIVATE, TRUE,
                 reinterpret_cast<LPARAM>(&toolInfo));
}

LRESULT CALLBACK ListTooltipSubclassProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR referenceData)
{
    auto& state = *reinterpret_cast<ListTooltipState*>(referenceData);
    switch (message)
    {
    case WM_MOUSEMOVE:
    {
        TRACKMOUSEEVENT tracking{};
        tracking.cbSize = sizeof(tracking);
        tracking.dwFlags = TME_LEAVE;
        tracking.hwndTrack = window;
        TrackMouseEvent(&tracking);
        UpdateListTooltip(
            state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        break;
    }
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        ResetListTooltip(state);
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(window, ListTooltipSubclassProcedure,
                             subclassId);
        break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

bool InitializeListTooltip(HWND window, HWND listView,
                           ListTooltipState& state,
                           UINT_PTR subclassId)
{
    HWND tooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        window, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (tooltip == nullptr)
    {
        return false;
    }
    SendMessageW(tooltip, CCM_SETUNICODEFORMAT, TRUE, 0);

    state.listView = listView;
    state.tooltip = tooltip;
    TOOLINFOW toolInfo{};
    // The application currently runs without a Common Controls v6 manifest.
    // Use the v2 structure size accepted by the system tooltip control.
    toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
    toolInfo.hwnd = window;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(listView);
    toolInfo.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE |
                      TTF_TRANSPARENT;
    toolInfo.lpszText = const_cast<wchar_t*>(L"");
    const bool toolAdded = SendMessageW(
        tooltip, TTM_ADDTOOLW, 0,
        reinterpret_cast<LPARAM>(&toolInfo)) != FALSE;
    const bool subclassInstalled = toolAdded && SetWindowSubclass(
        listView, ListTooltipSubclassProcedure, subclassId,
        reinterpret_cast<DWORD_PTR>(&state)) != FALSE;
    if (!subclassInstalled)
    {
        DestroyWindow(tooltip);
        state = {};
        return false;
    }
    SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 900);
    return true;
}

int GetEffectiveDurationSeconds(const Track& track)
{
    if (track.durationSeconds >= 0)
    {
        return track.durationSeconds;
    }
    if (track.extinfDuration >= 0)
    {
        return track.extinfDuration;
    }
    return -1;
}

std::wstring FormatTotalDuration(long long totalSeconds)
{
    const long long hours = totalSeconds / 3600;
    const long long minutes = (totalSeconds / 60) % 60;
    const long long seconds = totalSeconds % 60;
    std::wostringstream stream;
    stream << std::setfill(L'0') << std::setw(2) << hours << L':'
           << std::setw(2) << minutes << L':'
           << std::setw(2) << seconds;
    return stream.str();
}

struct DurationSummary
{
    std::size_t trackCount = 0;
    long long totalSeconds = 0;
    bool hasUnknownDuration = false;
};

DurationSummary SummarizeTracks(const Playlist& playlist,
                                const std::vector<int>* selectedIndices)
{
    DurationSummary summary{};
    const auto addTrack = [&summary](const Track& track) {
        ++summary.trackCount;
        const int duration = GetEffectiveDurationSeconds(track);
        if (duration >= 0)
        {
            summary.totalSeconds += duration;
        }
        else
        {
            summary.hasUnknownDuration = true;
        }
    };

    if (selectedIndices == nullptr)
    {
        for (const Track& track : playlist.tracks)
        {
            addTrack(track);
        }
    }
    else
    {
        for (const int index : *selectedIndices)
        {
            if (index >= 0 && index < static_cast<int>(playlist.tracks.size()))
            {
                addTrack(playlist.tracks[static_cast<std::size_t>(index)]);
            }
        }
    }
    return summary;
}

std::wstring FormatDurationSummary(const DurationSummary& summary)
{
    std::wstring text = std::to_wstring(summary.trackCount) +
                        T(UiText::TracksStatus) +
                        FormatTotalDuration(summary.totalSeconds);
    if (summary.hasUnknownDuration)
    {
        text += L'+';
    }
    return text;
}

void RefreshStatusBar()
{
    if (statusText == nullptr)
    {
        return;
    }

    DurationSummary selectedSummary{};
    DurationSummary playlistSummary{};
    if (const Playlist* playlist = GetSelectedPlaylist())
    {
        const std::vector<int> selectedIndices =
            GetSelectedTrackIndices(trackListView);
        selectedSummary = SummarizeTracks(*playlist, &selectedIndices);
        playlistSummary = SummarizeTracks(*playlist, nullptr);
    }

    const std::wstring text =
        std::wstring(T(UiText::SelectedStatus)) +
        FormatDurationSummary(selectedSummary) + L" | " +
        T(UiText::PlaylistStatus) + FormatDurationSummary(playlistSummary) +
        L"  ";
    SetWindowTextW(statusText, text.c_str());
}

std::vector<std::wstring> GetExistingTrackPaths(
    const std::vector<int>& trackIndices)
{
    std::vector<std::wstring> paths;
    const Playlist* playlist = GetSelectedPlaylist();
    if (playlist == nullptr)
    {
        return paths;
    }

    for (const int index : trackIndices)
    {
        if (index < 0 || index >= static_cast<int>(playlist->tracks.size()))
        {
            continue;
        }
        const std::wstring& path =
            playlist->tracks[static_cast<std::size_t>(index)].path;
        if (path.empty())
        {
            continue;
        }

        std::error_code error;
        if (std::filesystem::is_regular_file(std::filesystem::path(path), error) &&
            !error)
        {
            paths.push_back(path);
        }
    }
    return paths;
}

std::vector<std::wstring> GetSelectedExistingTrackPaths()
{
    return GetExistingTrackPaths(GetSelectedTrackIndices(trackListView));
}

bool CanOperateSelectedTracks(HWND listView)
{
    const int selectedCount = ListView_GetSelectedCount(listView);
    return selectedCount >= 1 && selectedCount <= 10;
}

bool HasSelectedPlaylist()
{
    return GetSelectedPlaylist() != nullptr;
}

bool HasSelectedTracks()
{
    return trackListView != nullptr &&
           ListView_GetSelectedCount(trackListView) > 0;
}

void SetMenuCommandEnabled(HMENU menu, UINT command, bool enabled)
{
    if (menu != nullptr)
    {
        EnableMenuItem(menu, command, MF_BYCOMMAND |
                       (enabled ? MF_ENABLED : MF_GRAYED));
    }
}

void UpdateMainMenuState()
{
    const bool hasPlaylist = HasSelectedPlaylist();
    const bool hasTracks = HasSelectedTracks();
    const bool canUseShellOperations = trackListView != nullptr &&
        CanOperateSelectedTracks(trackListView);

    SetMenuCommandEnabled(fileMenu, CommandExportM3U8, hasPlaylist);
    SetMenuCommandEnabled(playlistMenu, CommandRenamePlaylist, hasPlaylist);
    SetMenuCommandEnabled(playlistMenu, CommandDeletePlaylist, hasPlaylist);
    SetMenuCommandEnabled(trackMenu, CommandGetTrackMetadata, hasTracks);
    SetMenuCommandEnabled(trackMenu, CommandOpenTracksInExplorer,
                          canUseShellOperations);
    SetMenuCommandEnabled(trackMenu, CommandShowTrackProperties,
                          canUseShellOperations);
    SetMenuCommandEnabled(trackMenu, CommandDeleteTracks, hasTracks);

    if (trackSendToMenu != nullptr)
    {
        while (GetMenuItemCount(trackSendToMenu) > 0)
        {
            DeleteMenu(trackSendToMenu, 0, MF_BYPOSITION);
        }
        bool canUseAnySendToApplication = false;
        const int selectedTrackCount = trackListView == nullptr
            ? 0 : ListView_GetSelectedCount(trackListView);
        for (std::size_t index = 0; index < sendToApplications.size(); ++index)
        {
            const bool canUse = CanUseSendToApplication(
                sendToApplications[index], selectedTrackCount);
            canUseAnySendToApplication |= canUse;
            AppendMenuW(trackSendToMenu,
                        MF_STRING | (canUse ? MF_ENABLED : MF_GRAYED),
                        SendToApplicationCommandBase +
                            static_cast<UINT>(index),
                        sendToApplications[index].name.c_str());
        }
        EnableMenuItem(trackMenu, 3, MF_BYPOSITION |
            (hasTracks && canUseAnySendToApplication
                ? MF_ENABLED : MF_GRAYED));
    }
}

bool CreateMainMenuBar(HWND window)
{
    HMENU menuBar = CreateMenu();
    fileMenu = CreatePopupMenu();
    playlistMenu = CreatePopupMenu();
    trackMenu = CreatePopupMenu();
    trackSendToMenu = CreatePopupMenu();
    settingsMenu = CreatePopupMenu();
    helpMenu = CreatePopupMenu();
    languageMenu = CreatePopupMenu();
    if (menuBar == nullptr || fileMenu == nullptr ||
        playlistMenu == nullptr || trackMenu == nullptr ||
        trackSendToMenu == nullptr || settingsMenu == nullptr ||
        helpMenu == nullptr || languageMenu == nullptr)
    {
        if (menuBar != nullptr) DestroyMenu(menuBar);
        if (fileMenu != nullptr) DestroyMenu(fileMenu);
        if (playlistMenu != nullptr) DestroyMenu(playlistMenu);
        if (trackMenu != nullptr) DestroyMenu(trackMenu);
        if (trackSendToMenu != nullptr) DestroyMenu(trackSendToMenu);
        if (settingsMenu != nullptr) DestroyMenu(settingsMenu);
        if (helpMenu != nullptr) DestroyMenu(helpMenu);
        if (languageMenu != nullptr) DestroyMenu(languageMenu);
        fileMenu = nullptr;
        playlistMenu = nullptr;
        trackMenu = nullptr;
        trackSendToMenu = nullptr;
        settingsMenu = nullptr;
        helpMenu = nullptr;
        languageMenu = nullptr;
        return false;
    }

    AppendMenuW(fileMenu, MF_STRING, CommandOpenAudioFiles,
                T(UiText::OpenAudioFiles));
    AppendMenuW(fileMenu, MF_STRING, CommandImportPlaylist,
                T(UiText::ImportPlaylist));
    AppendMenuW(fileMenu, MF_STRING, CommandExportM3U8,
                T(UiText::ExportM3U8));
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, CommandExit, T(UiText::Exit));

    AppendMenuW(playlistMenu, MF_STRING, CommandNewPlaylist,
                T(UiText::NewPlaylist));
    AppendMenuW(playlistMenu, MF_STRING, CommandRenamePlaylist,
                T(UiText::RenamePlaylist));
    AppendMenuW(playlistMenu, MF_STRING, CommandDeletePlaylist,
                T(UiText::DeletePlaylist));
    AppendMenuW(playlistMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(playlistMenu, MF_STRING, CommandOrganizePlaylists,
                T(UiText::OrganizePlaylists));

    AppendMenuW(trackMenu, MF_STRING, CommandGetTrackMetadata,
                T(UiText::GetMetadata));
    AppendMenuW(trackMenu, MF_STRING, CommandOpenTracksInExplorer,
                T(UiText::OpenInExplorer));
    AppendMenuW(trackMenu, MF_STRING, CommandShowTrackProperties,
                T(UiText::Properties));
    AppendMenuW(trackMenu, MF_POPUP,
                reinterpret_cast<UINT_PTR>(trackSendToMenu), T(UiText::SendTo));
    AppendMenuW(trackMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(trackMenu, MF_STRING, CommandDeleteTracks, T(UiText::Delete));

    AppendMenuW(languageMenu, MF_STRING, CommandLanguageEnglish,
                T(UiText::EnglishLanguage));
    AppendMenuW(languageMenu, MF_STRING, CommandLanguageJapanese,
                T(UiText::JapaneseLanguage));
    CheckMenuRadioItem(
        languageMenu, CommandLanguageEnglish, CommandLanguageJapanese,
        languageSetting == AppLanguage::Japanese
            ? CommandLanguageJapanese : CommandLanguageEnglish,
        MF_BYCOMMAND);
    AppendMenuW(settingsMenu, MF_POPUP,
                reinterpret_cast<UINT_PTR>(languageMenu),
                T(UiText::LanguageMenu));

    AppendMenuW(settingsMenu, MF_STRING, CommandSendToApplications,
                T(UiText::SendToApplications));
    AppendMenuW(settingsMenu, MF_STRING, CommandTrackColumns,
                T(UiText::Columns));
    AppendMenuW(settingsMenu, MF_STRING, CommandExtinfFormat,
                T(UiText::ExtinfFormat));

    AppendMenuW(helpMenu, MF_STRING, CommandOnlineManual,
                T(UiText::OnlineManual));
    AppendMenuW(helpMenu, MF_STRING, CommandAbout,
                T(UiText::AboutPlaylistManager));

    AppendMenuW(menuBar, MF_POPUP,
                reinterpret_cast<UINT_PTR>(fileMenu), T(UiText::FileMenu));
    AppendMenuW(menuBar, MF_POPUP,
                reinterpret_cast<UINT_PTR>(playlistMenu), T(UiText::PlaylistMenu));
    AppendMenuW(menuBar, MF_POPUP,
                reinterpret_cast<UINT_PTR>(trackMenu), T(UiText::TrackMenu));
    AppendMenuW(menuBar, MF_POPUP,
                reinterpret_cast<UINT_PTR>(settingsMenu), T(UiText::SettingsMenu));
    AppendMenuW(menuBar, MF_POPUP,
                reinterpret_cast<UINT_PTR>(helpMenu), T(UiText::HelpMenu));

    if (!SetMenu(window, menuBar))
    {
        DestroyMenu(menuBar);
        fileMenu = nullptr;
        playlistMenu = nullptr;
        trackMenu = nullptr;
        trackSendToMenu = nullptr;
        settingsMenu = nullptr;
        helpMenu = nullptr;
        languageMenu = nullptr;
        return false;
    }
    UpdateMainMenuState();
    return true;
}

bool IsUsableTrackFile(const std::wstring& path)
{
    if (path.empty())
    {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(std::filesystem::path(path), error) &&
           !error;
}

std::wstring QuoteWindowsCommandLineArgument(const std::wstring& value)
{
    std::wstring quoted = L"\"";
    std::size_t backslashCount = 0;
    for (const wchar_t character : value)
    {
        if (character == L'\\')
        {
            ++backslashCount;
            continue;
        }
        if (character == L'\"')
        {
            quoted.append(backslashCount * 2 + 1, L'\\');
            quoted += L'\"';
        }
        else
        {
            quoted.append(backslashCount, L'\\');
            quoted += character;
        }
        backslashCount = 0;
    }
    quoted.append(backslashCount * 2, L'\\');
    quoted += L'\"';
    return quoted;
}

std::wstring BuildSendToFilesArgument(
    const std::vector<std::wstring>& paths)
{
    std::wstring arguments;
    for (const std::wstring& path : paths)
    {
        if (!arguments.empty())
        {
            arguments += L' ';
        }
        arguments += QuoteWindowsCommandLineArgument(path);
    }
    return arguments;
}

bool ExpandSendToArguments(const std::wstring& argumentsTemplate,
                           const std::vector<std::wstring>& paths,
                           std::wstring& expandedArguments)
{
    const SendToArgumentMode mode =
        GetSendToArgumentMode(argumentsTemplate);
    const wchar_t* placeholder = nullptr;
    std::size_t placeholderLength = 0;
    std::wstring replacement;
    if (mode == SendToArgumentMode::Files)
    {
        if (paths.empty())
        {
            return false;
        }
        placeholder = L"%files%";
        placeholderLength = 7;
        replacement = BuildSendToFilesArgument(paths);
    }
    else if (mode == SendToArgumentMode::Folder)
    {
        if (paths.size() != 1)
        {
            return false;
        }
        placeholder = L"%folder%";
        placeholderLength = 8;
        replacement = std::filesystem::path(paths.front())
                          .parent_path().wstring();
        if (replacement.empty())
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    const std::size_t placeholderPosition =
        argumentsTemplate.find(placeholder);
    expandedArguments = argumentsTemplate;
    expandedArguments.replace(placeholderPosition, placeholderLength,
                              replacement);
    return true;
}

void ShowSendToFailure(HWND owner, const SendToApplication& application,
                       const std::wstring& detail = L"")
{
    std::wstring message = T(UiText::SendFailedPrefix) +
        application.name + T(UiText::SendFailedSuffix);
    if (!detail.empty())
    {
        message += L"\n\n" + detail;
    }
    MessageBoxW(owner, message.c_str(), WindowTitle,
                MB_OK | MB_ICONERROR);
}

std::wstring GetWindowsErrorMessage(DWORD errorCode)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, reinterpret_cast<LPWSTR>(&buffer), 0,
        nullptr);
    std::wstring message;
    if (length > 0 && buffer != nullptr)
    {
        message.assign(buffer, length);
        while (!message.empty() &&
               (message.back() == L'\r' || message.back() == L'\n'))
        {
            message.pop_back();
        }
    }
    if (buffer != nullptr)
    {
        LocalFree(buffer);
    }
    return message;
}

bool SendTrackPathsToApplication(HWND owner,
                                 const std::vector<std::wstring>& paths,
                                 int applicationIndex)
{
    if (applicationIndex < 0 ||
        applicationIndex >= static_cast<int>(sendToApplications.size()))
    {
        return false;
    }
    const SendToApplication application =
        sendToApplications[static_cast<std::size_t>(applicationIndex)];
    const SendToArgumentMode mode =
        GetSendToArgumentMode(application.arguments);
    if (mode == SendToArgumentMode::Invalid)
    {
        ShowSendToFailure(
            owner, application,
            T(UiText::ArgumentsPlaceholderRequired));
        return false;
    }
    if (!CanUseSendToApplication(
            application, static_cast<int>(paths.size())))
    {
        return false;
    }
    if (paths.empty())
    {
        return false;
    }
    if (std::any_of(paths.begin(), paths.end(),
                    [](const std::wstring& path) {
                        return !IsUsableTrackFile(path);
                    }))
    {
        return false;
    }
    if (mode == SendToArgumentMode::Folder)
    {
        const std::filesystem::path folder =
            std::filesystem::path(paths.front()).parent_path();
        std::error_code error;
        if (folder.empty() || !std::filesystem::is_directory(folder, error) ||
            error)
        {
            return false;
        }
    }
    if (!IsUsableTrackFile(application.executablePath))
    {
        ShowSendToFailure(owner, application,
                          T(UiText::ConfiguredExecutableMissing));
        return false;
    }
    if (!IsValidSendToArguments(application.arguments))
    {
        ShowSendToFailure(
            owner, application,
            T(UiText::ArgumentsPlaceholderRequired));
        return false;
    }

    std::wstring expandedArguments;
    if (!ExpandSendToArguments(application.arguments, paths,
                               expandedArguments))
    {
        ShowSendToFailure(owner, application,
                          T(UiText::SelectedTracksExpandFailed));
        return false;
    }

    std::wstring commandLine = QuoteWindowsCommandLineArgument(
        application.executablePath);
    if (!expandedArguments.empty())
    {
        commandLine += L' ';
        commandLine += expandedArguments;
    }
    if (commandLine.size() + 1 > MaximumWindowsCommandLineLength)
    {
        ShowSendToFailure(owner, application,
                          T(UiText::TooManyTracks));
        return false;
    }

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(),
                                             commandLine.end());
    mutableCommandLine.push_back(L'\0');
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessW(application.executablePath.c_str(),
                        mutableCommandLine.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &startupInfo, &processInfo))
    {
        const DWORD errorCode = GetLastError();
        std::wstring detail = GetWindowsErrorMessage(errorCode);
        if (detail.empty())
        {
            detail = std::wstring(T(UiText::WindowsErrorPrefix)) +
                std::to_wstring(errorCode) + L".";
        }
        ShowSendToFailure(owner, application, detail);
        return false;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

bool SendTracksToApplication(HWND owner,
                             const std::vector<int>& trackIndices,
                             int applicationIndex)
{
    return SendTrackPathsToApplication(
        owner, GetExistingTrackPaths(trackIndices), applicationIndex);
}

bool SendSelectedTracksToApplication(HWND owner, int applicationIndex)
{
    return SendTracksToApplication(
        owner, GetSelectedTrackIndices(trackListView), applicationIndex);
}

bool OpenFileInExplorer(const std::wstring& path)
{
    if (!IsUsableTrackFile(path))
    {
        return false;
    }

    PIDLIST_ABSOLUTE fileItemId = nullptr;
    if (FAILED(SHParseDisplayName(path.c_str(), nullptr, &fileItemId,
                                  0, nullptr)) ||
        fileItemId == nullptr)
    {
        return false;
    }

    PIDLIST_ABSOLUTE folderItemId = ILCloneFull(fileItemId);
    if (folderItemId == nullptr || !ILRemoveLastID(folderItemId))
    {
        CoTaskMemFree(folderItemId);
        CoTaskMemFree(fileItemId);
        return false;
    }

    PCUITEMID_CHILD childItemId = ILFindLastID(fileItemId);
    const HRESULT result = SHOpenFolderAndSelectItems(
        folderItemId, 1, &childItemId, 0);
    CoTaskMemFree(folderItemId);
    CoTaskMemFree(fileItemId);
    return SUCCEEDED(result);
}

void OpenSelectedTracksInExplorer()
{
    Playlist* playlist = GetSelectedPlaylist();
    const std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    if (playlist == nullptr || !CanOperateSelectedTracks(trackListView))
    {
        return;
    }

    for (const int index : selectedIndices)
    {
        if (index >= 0 && index < static_cast<int>(playlist->tracks.size()))
        {
            OpenFileInExplorer(
                playlist->tracks[static_cast<std::size_t>(index)].path);
        }
    }
}

bool ShowFileProperties(const std::wstring& path)
{
    if (!IsUsableTrackFile(path))
    {
        return false;
    }

    SHELLEXECUTEINFOW shellExecute{};
    shellExecute.cbSize = sizeof(shellExecute);
    shellExecute.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
    shellExecute.hwnd = mainWindow;
    shellExecute.lpVerb = L"properties";
    shellExecute.lpFile = path.c_str();
    shellExecute.nShow = SW_SHOW;
    return ShellExecuteExW(&shellExecute) != FALSE;
}

void ShowSelectedTrackProperties()
{
    Playlist* playlist = GetSelectedPlaylist();
    const std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    if (playlist == nullptr || !CanOperateSelectedTracks(trackListView))
    {
        return;
    }

    for (const int index : selectedIndices)
    {
        if (index >= 0 && index < static_cast<int>(playlist->tracks.size()))
        {
            ShowFileProperties(
                playlist->tracks[static_cast<std::size_t>(index)].path);
        }
    }
}

void StartSelectedTrackDrag(int dragItemIndex)
{
    if (!oleDragDropAvailable || GetSelectedPlaylist() == nullptr ||
        dragItemIndex < 0)
    {
        return;
    }

    if ((ListView_GetItemState(trackListView, dragItemIndex, LVIS_SELECTED) &
        LVIS_SELECTED) == 0)
    {
        ListView_SetItemState(trackListView, -1, 0, LVIS_SELECTED);
        ListView_SetItemState(trackListView, dragItemIndex,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    }

    const std::vector<std::wstring> paths = GetSelectedExistingTrackPaths();
    if (!paths.empty())
    {
        StartExternalFileDrag(paths);
    }
}

void RestoreTrackSelection(const std::vector<int>& indices)
{
    const int itemCount = ListView_GetItemCount(trackListView);
    int firstSelected = -1;
    for (const int index : indices)
    {
        if (index >= 0 && index < itemCount)
        {
            ListView_SetItemState(trackListView, index,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            if (firstSelected == -1)
            {
                firstSelected = index;
            }
        }
    }
    if (firstSelected >= 0)
    {
        ListView_EnsureVisible(trackListView, firstSelected, FALSE);
    }
}

void SaveCurrentTrackColumnWidths()
{
    if (trackListView == nullptr)
    {
        return;
    }
    for (std::size_t index = 0; index < visibleTrackColumnIds.size(); ++index)
    {
        const int width = ListView_GetColumnWidth(
            trackListView, static_cast<int>(index));
        TrackColumnConfig* config = FindTrackColumnConfig(
            visibleTrackColumnIds[index]);
        if (config != nullptr && width >= 24 && width <= 4096)
        {
            config->width = width;
        }
    }
}

void RebuildTrackListColumns()
{
    if (trackListView == nullptr)
    {
        return;
    }
    SaveCurrentTrackColumnWidths();
    const std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    while (ListView_DeleteColumn(trackListView, 0))
    {
    }
    visibleTrackColumnIds.clear();
    for (const TrackColumnConfig& config : trackColumnConfigs)
    {
        if (!config.visible)
        {
            continue;
        }
        const int index = static_cast<int>(visibleTrackColumnIds.size());
        const std::wstring name = GetTrackColumnName(config.id);
        InsertColumn(trackListView, index, name.c_str(), config.width);
        visibleTrackColumnIds.push_back(config.id);
    }
    RefreshSelectedTrackList();
    RestoreTrackSelection(selectedIndices);
}

bool AreIndicesContiguous(const std::vector<int>& indices)
{
    if (indices.empty())
    {
        return false;
    }
    for (std::size_t index = 1; index < indices.size(); ++index)
    {
        if (indices[index] != indices.front() + static_cast<int>(index))
        {
            return false;
        }
    }
    return true;
}

bool MoveSelectedPlaylist(int direction)
{
    if ((direction != -1 && direction != 1) ||
        selectedPlaylistIndex < 0 ||
        selectedPlaylistIndex >= static_cast<int>(playlists.size()))
    {
        return false;
    }

    const int selectedRow = ListView_GetNextItem(
        playlistListView, -1, LVNI_SELECTED);
    if (GetPlaylistIndexFromVisibleRow(selectedRow) != selectedPlaylistIndex)
    {
        return false;
    }

    const int groupId = playlists[
        static_cast<std::size_t>(selectedPlaylistIndex)].groupId;
    int destinationIndex = selectedPlaylistIndex + direction;
    while (destinationIndex >= 0 &&
           destinationIndex < static_cast<int>(playlists.size()) &&
           playlists[static_cast<std::size_t>(destinationIndex)].groupId !=
               groupId)
    {
        destinationIndex += direction;
    }
    if (destinationIndex < 0 ||
        destinationIndex >= static_cast<int>(playlists.size()))
    {
        return false;
    }

    std::swap(playlists[static_cast<std::size_t>(selectedPlaylistIndex)],
              playlists[static_cast<std::size_t>(destinationIndex)]);
    selectedPlaylistIndex = destinationIndex;
    RefreshPlaylistList(playlistListView);
    SetFocus(playlistListView);
    MarkAppStateDirty();
    return true;
}

bool MoveSelectedTracks(int direction)
{
    Playlist* playlist = GetSelectedPlaylist();
    const std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    if ((direction != -1 && direction != 1) || playlist == nullptr ||
        !AreIndicesContiguous(selectedIndices))
    {
        return false;
    }

    const int firstSelected = selectedIndices.front();
    const int lastSelected = selectedIndices.back();
    const int trackCount = static_cast<int>(playlist->tracks.size());
    if (firstSelected < 0 || lastSelected >= trackCount ||
        (direction < 0 && firstSelected == 0) ||
        (direction > 0 && lastSelected == trackCount - 1) ||
        firstSelected > lastSelected)
    {
        return false;
    }

    auto begin = playlist->tracks.begin();
    if (direction < 0)
    {
        std::rotate(begin + firstSelected - 1, begin + firstSelected,
                    begin + lastSelected + 1);
    }
    else
    {
        std::rotate(begin + firstSelected, begin + lastSelected + 1,
                    begin + lastSelected + 2);
    }

    std::vector<int> movedIndices = selectedIndices;
    for (int& index : movedIndices)
    {
        index += direction;
    }
    playlist->isModified = true;
    RefreshSelectedTrackList();
    RestoreTrackSelection(movedIndices);
    SetFocus(trackListView);
    MarkAppStateDirty();
    return true;
}

bool HandleAltArrowKey(const MSG& message)
{
    if ((message.message != WM_KEYDOWN &&
         message.message != WM_SYSKEYDOWN) ||
        (message.wParam != VK_UP && message.wParam != VK_DOWN) ||
        (GetKeyState(VK_MENU) & 0x8000) == 0 ||
        (GetKeyState(VK_CONTROL) & 0x8000) != 0 ||
        (GetKeyState(VK_SHIFT) & 0x8000) != 0)
    {
        return false;
    }

    const int direction = message.wParam == VK_UP ? -1 : 1;
    const HWND focusedWindow = GetFocus();
    if (focusedWindow == playlistListView)
    {
        MoveSelectedPlaylist(direction);
        return true;
    }
    if (focusedWindow == trackListView)
    {
        MoveSelectedTracks(direction);
        return true;
    }
    return false;
}

void DeleteSelectedTracks()
{
    Playlist* playlist = GetSelectedPlaylist();
    std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    if (playlist == nullptr || selectedIndices.empty())
    {
        return;
    }

    const int nextSelection = selectedIndices.front();
    for (auto iterator = selectedIndices.rbegin();
         iterator != selectedIndices.rend(); ++iterator)
    {
        const int index = *iterator;
        if (index >= 0 && index < static_cast<int>(playlist->tracks.size()))
        {
            playlist->tracks.erase(playlist->tracks.begin() + index);
        }
    }
    playlist->isModified = true;
    MarkAppStateDirty();
    RefreshSelectedTrackList();

    if (!playlist->tracks.empty())
    {
        const int correctedSelection = std::min(
            nextSelection, static_cast<int>(playlist->tracks.size()) - 1);
        RestoreTrackSelection({correctedSelection});
    }
}

void GetMetadataForSelectedTracks()
{
    Playlist* playlist = GetSelectedPlaylist();
    const std::vector<int> selectedIndices =
        GetSelectedTrackIndices(trackListView);
    if (playlist == nullptr || selectedIndices.empty())
    {
        return;
    }

    MetadataRequest request{};
    request.title = IsTrackColumnVisible(TrackColumnId::Title);
    request.artist = IsTrackColumnVisible(TrackColumnId::Artist);
    request.album = IsTrackColumnVisible(TrackColumnId::Album);
    request.comment = IsTrackColumnVisible(TrackColumnId::Comment);
    request.trackNumber = IsTrackColumnVisible(TrackColumnId::TrackNumber);
    request.year = IsTrackColumnVisible(TrackColumnId::Year);
    request.genre = IsTrackColumnVisible(TrackColumnId::Genre);
    request.albumArtist = IsTrackColumnVisible(TrackColumnId::AlbumArtist);
    request.discNumber = IsTrackColumnVisible(TrackColumnId::DiscNumber);
    request.duration = IsTrackColumnVisible(TrackColumnId::Duration);
    request.format = IsTrackColumnVisible(TrackColumnId::Format);
    request.bitrate = IsTrackColumnVisible(TrackColumnId::Bitrate);
    request.sampleRate = IsTrackColumnVisible(TrackColumnId::SampleRate);
    request.fileSize = IsTrackColumnVisible(TrackColumnId::FileSize);
    request.dateModified = IsTrackColumnVisible(TrackColumnId::DateModified);

    bool updatedAnyTrack = false;
    for (const int index : selectedIndices)
    {
        if (index >= 0 && index < static_cast<int>(playlist->tracks.size()))
        {
            updatedAnyTrack =
                UpdateTrackMetadata(
                    playlist->tracks[static_cast<std::size_t>(index)],
                    request) ||
                updatedAnyTrack;
        }
    }
    if (updatedAnyTrack)
    {
        playlist->isModified = true;
        MarkAppStateDirty();
    }
    RefreshSelectedTrackList();
    RestoreTrackSelection(selectedIndices);
}

bool HasVisibleName(const std::wstring& name)
{
    return std::any_of(name.begin(), name.end(), [](wchar_t character) {
        return std::iswspace(character) == 0;
    });
}

std::wstring MakeNewPlaylistName()
{
    const std::wstring baseName = L"New Playlist";
    const auto nameExists = [](const std::wstring& candidate) {
        return std::any_of(playlists.begin(), playlists.end(),
                           [&candidate](const Playlist& playlist) {
                               return playlist.name == candidate;
                           });
    };

    if (!nameExists(baseName))
    {
        return baseName;
    }

    for (int number = 2;; ++number)
    {
        std::wstring candidate = baseName + L" " + std::to_wstring(number);
        if (!nameExists(candidate))
        {
            return candidate;
        }
    }
}

void CreateNewPlaylist()
{
    playlists.push_back(Playlist{MakeNewPlaylistName(), L"",
                                 NewPlaylistGroupId, {}, false});
    selectedPlaylistIndex = static_cast<int>(playlists.size()) - 1;
    EnsurePlaylistGroupExpanded(NewPlaylistGroupId);
    MarkAppStateDirty();
    RefreshPlaylistList(playlistListView);
    RefreshSelectedTrackList();
    SetFocus(playlistListView);
    const int selectedRow = FindVisibleRowForPlaylist(selectedPlaylistIndex);
    if (selectedRow >= 0)
    {
        ListView_EditLabel(playlistListView, selectedRow);
    }
}

void RenameSelectedPlaylist()
{
    if (GetSelectedPlaylist() == nullptr)
    {
        return;
    }

    const int groupId = GetSelectedPlaylist()->groupId;
    if (EnsurePlaylistGroupExpanded(groupId))
    {
        MarkAppStateDirty();
        RefreshPlaylistList(playlistListView);
    }
    const int selectedRow = FindVisibleRowForPlaylist(selectedPlaylistIndex);
    if (selectedRow >= 0)
    {
        SetFocus(playlistListView);
        ListView_EditLabel(playlistListView, selectedRow);
    }
}

void DeleteSelectedPlaylist(HWND window)
{
    Playlist* selectedPlaylist = GetSelectedPlaylist();
    if (selectedPlaylist == nullptr)
    {
        return;
    }

    if (!selectedPlaylist->tracks.empty())
    {
        const std::wstring message =
            T(UiText::DeletePlaylistQuestionPrefix) +
            selectedPlaylist->name + T(UiText::DeletePlaylistQuestionSuffix) +
            L"\n\n" + T(UiText::AudioFilesNotDeleted);
        if (MessageBoxW(window, message.c_str(), WindowTitle,
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return;
        }
    }

    playlists.erase(playlists.begin() + selectedPlaylistIndex);
    if (playlists.empty())
    {
        selectedPlaylistIndex = -1;
    }
    else if (selectedPlaylistIndex >= static_cast<int>(playlists.size()))
    {
        selectedPlaylistIndex = static_cast<int>(playlists.size()) - 1;
    }

    MarkAppStateDirty();
    RefreshPlaylistList(playlistListView);
    RefreshSelectedTrackList();
}

std::wstring MakeSafeExportFileName(const std::wstring& playlistName)
{
    std::wstring fileName = playlistName;
    constexpr wchar_t InvalidFileNameCharacters[] = L"<>:\"/\\|?*";
    for (wchar_t& character : fileName)
    {
        if (std::wcschr(InvalidFileNameCharacters, character) != nullptr)
        {
            character = L'_';
        }
    }
    while (!fileName.empty() &&
           (fileName.back() == L' ' || fileName.back() == L'.'))
    {
        fileName.pop_back();
    }
    if (fileName.empty())
    {
        fileName = L"Playlist";
    }
    return fileName + L".m3u8";
}

void ExportSelectedPlaylist(HWND window)
{
    Playlist* playlist = GetSelectedPlaylist();
    if (playlist == nullptr)
    {
        MessageBoxW(window, T(UiText::NoPlaylistSelected), WindowTitle,
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    constexpr DWORD MaximumPathLength = 32768;
    std::vector<wchar_t> filePathBuffer(MaximumPathLength, L'\0');
    std::wstring initialPath = playlist->filePath.empty()
        ? MakeSafeExportFileName(playlist->name)
        : playlist->filePath;
    if (!IsM3U8Path(initialPath))
    {
        std::filesystem::path exportPath(initialPath);
        exportPath.replace_extension(L".m3u8");
        initialPath = exportPath.wstring();
    }
    const std::size_t copyLength =
        std::min(initialPath.size(), filePathBuffer.size() - 1);
    std::copy_n(initialPath.data(), copyLength, filePathBuffer.data());

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = T(UiText::M3U8FilesFilter);
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = filePathBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(filePathBuffer.size());
    dialog.lpstrDefExt = L"m3u8";
    dialog.lpstrTitle = T(UiText::ExportPlaylistTitle);
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(window, T(UiText::SaveDialogFailed),
                        WindowTitle, MB_OK | MB_ICONERROR);
        }
        return;
    }

    try
    {
        SaveM3U8(*playlist, filePathBuffer.data(), extinfFormatPreset,
                 customExtinfFormat);
        playlist->filePath = filePathBuffer.data();
        playlist->isModified = false;
        MarkAppStateDirty();
        MessageBoxW(window, T(UiText::ExportSucceeded),
                    WindowTitle, MB_OK | MB_ICONINFORMATION);
    }
    catch (const std::exception&)
    {
        MessageBoxW(window, T(UiText::ExportFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
    }
}

void SelectPlaylist(int index)
{
    if (index < 0 || index >= static_cast<int>(playlists.size()))
    {
        return;
    }

    if (selectedPlaylistIndex != index)
    {
        selectedPlaylistIndex = index;
        MarkAppStateDirty();
    }
    ListView_SetItemState(playlistListView, -1, 0,
                          LVIS_SELECTED | LVIS_FOCUSED);
    const int visibleRow = FindVisibleRowForPlaylist(index);
    if (visibleRow >= 0)
    {
        ListView_SetItemState(playlistListView, visibleRow,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    }
    RefreshSelectedTrackList();
}

void ShowPlaylistContextMenu(HWND window, LPARAM lParam)
{
    POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    bool contextHasPlaylist = false;
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        RECT itemRect{};
        const int selectedRow =
            FindVisibleRowForPlaylist(selectedPlaylistIndex);
        if (selectedRow >= 0 &&
            ListView_GetItemRect(playlistListView, selectedRow,
                                 &itemRect, LVIR_BOUNDS))
        {
            screenPoint = {itemRect.left, itemRect.bottom};
            ClientToScreen(playlistListView, &screenPoint);
            contextHasPlaylist = true;
        }
        else
        {
            RECT listRect{};
            GetWindowRect(playlistListView, &listRect);
            screenPoint = {listRect.left + 8, listRect.top + 8};
        }
    }
    else
    {
        POINT listPoint = screenPoint;
        ScreenToClient(playlistListView, &listPoint);
        LVHITTESTINFO hitTest{};
        hitTest.pt = listPoint;
        const int hitIndex = ListView_HitTest(playlistListView, &hitTest);
        const int playlistIndex =
            GetPlaylistIndexFromVisibleRow(hitIndex);
        if (playlistIndex >= 0)
        {
            SelectPlaylist(playlistIndex);
            contextHasPlaylist = true;
        }
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING, CommandNewPlaylist,
                T(UiText::NewPlaylist));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const UINT selectionState = contextHasPlaylist
        ? MF_ENABLED
        : MF_GRAYED;
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandRenamePlaylist, T(UiText::RenamePlaylist));
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandDeletePlaylist, T(UiText::DeletePlaylist));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandExportM3U8, T(UiText::ExportM3U8));

    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command != 0)
    {
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

void ShowTrackContextMenu(HWND window, LPARAM lParam)
{
    POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        const std::vector<int> selectedIndices =
            GetSelectedTrackIndices(trackListView);
        RECT itemRect{};
        if (!selectedIndices.empty() &&
            ListView_GetItemRect(trackListView, selectedIndices.front(),
                                 &itemRect, LVIR_BOUNDS))
        {
            screenPoint = {itemRect.left, itemRect.bottom};
            ClientToScreen(trackListView, &screenPoint);
        }
        else
        {
            RECT listRect{};
            GetWindowRect(trackListView, &listRect);
            screenPoint = {listRect.left + 8, listRect.top + 8};
        }
    }
    else
    {
        POINT listPoint = screenPoint;
        ScreenToClient(trackListView, &listPoint);
        LVHITTESTINFO hitTest{};
        hitTest.pt = listPoint;
        const int hitIndex = ListView_HitTest(trackListView, &hitTest);
        if (hitIndex >= 0 &&
            (ListView_GetItemState(trackListView, hitIndex, LVIS_SELECTED) &
             LVIS_SELECTED) == 0)
        {
            ListView_SetItemState(trackListView, -1, 0, LVIS_SELECTED);
            RestoreTrackSelection({hitIndex});
        }
    }
    SetFocus(trackListView);

    HMENU menu = CreatePopupMenu();
    HMENU sendToMenu = CreatePopupMenu();
    if (menu == nullptr || sendToMenu == nullptr)
    {
        if (sendToMenu != nullptr)
        {
            DestroyMenu(sendToMenu);
        }
        if (menu != nullptr)
        {
            DestroyMenu(menu);
        }
        return;
    }

    const int selectedCount = ListView_GetSelectedCount(trackListView);
    const UINT selectionState = selectedCount == 0 ? MF_GRAYED : MF_ENABLED;
    const UINT shellOperationState = CanOperateSelectedTracks(trackListView)
        ? MF_ENABLED
        : MF_GRAYED;
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandGetTrackMetadata, T(UiText::GetMetadata));
    AppendMenuW(menu, MF_STRING | shellOperationState,
                CommandOpenTracksInExplorer, T(UiText::OpenInExplorer));
    AppendMenuW(menu, MF_STRING | shellOperationState,
                CommandShowTrackProperties, T(UiText::Properties));
    for (std::size_t index = 0; index < sendToApplications.size(); ++index)
    {
        const bool canUse = CanUseSendToApplication(
            sendToApplications[index], selectedCount);
        AppendMenuW(sendToMenu,
                    MF_STRING | (canUse ? MF_ENABLED : MF_GRAYED),
                    SendToApplicationCommandBase +
                        static_cast<UINT>(index),
                    sendToApplications[index].name.c_str());
    }
    const bool canUseAnySendToApplication = std::any_of(
        sendToApplications.begin(), sendToApplications.end(),
        [selectedCount](const SendToApplication& application)
        {
            return CanUseSendToApplication(application, selectedCount);
        });
    const UINT sendToState = canUseAnySendToApplication
        ? MF_ENABLED : MF_GRAYED;
    AppendMenuW(menu, MF_POPUP | sendToState,
                reinterpret_cast<UINT_PTR>(sendToMenu), T(UiText::SendTo));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandDeleteTracks, T(UiText::Delete));

    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command != 0)
    {
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

void LayoutChildren(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int paneHeight = std::max(0, height - statusHeight);

    if (width >= MinimumPaneWidth * 2 + SplitterWidth)
    {
        splitterX = std::clamp(splitterX, MinimumPaneWidth,
                               width - MinimumPaneWidth - SplitterWidth);
    }
    else
    {
        splitterX = std::max(0, (width - SplitterWidth) / 2);
    }

    const int rightX = std::min(width, splitterX + SplitterWidth);
    const int rightWidth = std::max(0, width - rightX);

    MoveWindow(playlistListView, 0, 0, splitterX, paneHeight, TRUE);
    MoveWindow(trackListView, rightX, 0, rightWidth, paneHeight, TRUE);
    MoveWindow(statusText, 0, paneHeight, width, statusHeight, TRUE);

    ListView_SetColumnWidth(playlistListView, 0, std::max(0, splitterX - 4));
    InvalidateRect(window, nullptr, FALSE);
}

void InitializeStatusFont()
{
    LOGFONTW statusLogFont{};
    const HFONT defaultFont = static_cast<HFONT>(
        GetStockObject(DEFAULT_GUI_FONT));
    HDC statusDeviceContext = GetDC(statusText);
    const int dpi = statusDeviceContext == nullptr
        ? 96
        : GetDeviceCaps(statusDeviceContext, LOGPIXELSY);
    if (defaultFont != nullptr &&
        GetObjectW(defaultFont, sizeof(statusLogFont), &statusLogFont) != 0)
    {
        statusLogFont.lfHeight = -MulDiv(11, dpi <= 0 ? 96 : dpi, 72);
        statusFont = CreateFontIndirectW(&statusLogFont);
    }

    const HFONT displayFont = statusFont != nullptr ? statusFont : defaultFont;
    SendMessageW(statusText, WM_SETFONT,
                 reinterpret_cast<WPARAM>(displayFont), TRUE);
    if (statusDeviceContext != nullptr)
    {
        const HGDIOBJ previousFont = SelectObject(statusDeviceContext,
                                                   displayFont);
        TEXTMETRICW metrics{};
        if (GetTextMetricsW(statusDeviceContext, &metrics))
        {
            statusHeight = metrics.tmHeight + 10;
        }
        SelectObject(statusDeviceContext, previousFont);
        ReleaseDC(statusText, statusDeviceContext);
    }
}

bool IsPointOnSplitter(HWND window, POINT point)
{
    RECT client{};
    GetClientRect(window, &client);
    return point.x >= splitterX && point.x < splitterX + SplitterWidth &&
           point.y >= 0 && point.y < client.bottom - statusHeight;
}

bool IsPointInTrackPane(HWND window, POINT point)
{
    RECT trackRect{};
    GetWindowRect(trackListView, &trackRect);
    MapWindowPoints(HWND_DESKTOP, window,
                    reinterpret_cast<POINT*>(&trackRect), 2);
    return PtInRect(&trackRect, point) != FALSE;
}

enum class PlaylistLoadMode
{
    Cancel,
    NewPlaylist,
    AppendToCurrent
};

PlaylistLoadMode ChoosePlaylistLoadMode(HWND window,
                                        const std::wstring& filePath)
{
    if (GetSelectedPlaylist() == nullptr)
    {
        const std::wstring message =
            T(UiText::LoadNewPlaylistPrompt) + filePath;
        return MessageBoxW(window, message.c_str(), WindowTitle,
                           MB_OKCANCEL | MB_ICONQUESTION) == IDOK
            ? PlaylistLoadMode::NewPlaylist
            : PlaylistLoadMode::Cancel;
    }

    const std::wstring message =
        T(UiText::LoadPlaylistPrompt) + filePath +
        T(UiText::LoadPlaylistOptions);
    switch (MessageBoxW(window, message.c_str(), WindowTitle,
                        MB_YESNOCANCEL | MB_ICONQUESTION))
    {
    case IDYES:
        return PlaylistLoadMode::NewPlaylist;
    case IDNO:
        return PlaylistLoadMode::AppendToCurrent;
    default:
        return PlaylistLoadMode::Cancel;
    }
}

void ImportPlaylistFile(HWND window, const std::wstring& filePath)
{
    const PlaylistLoadMode mode = ChoosePlaylistLoadMode(window, filePath);
    if (mode == PlaylistLoadMode::Cancel)
    {
        return;
    }

    Playlist loadedPlaylist;
    try
    {
        loadedPlaylist = LoadPlaylist(filePath);
    }
    catch (const std::exception&)
    {
        MessageBoxW(window, T(UiText::LoadM3U8Failed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    if (mode == PlaylistLoadMode::NewPlaylist)
    {
        loadedPlaylist.groupId = NewPlaylistGroupId;
        playlists.push_back(std::move(loadedPlaylist));
        selectedPlaylistIndex = static_cast<int>(playlists.size()) - 1;
        EnsurePlaylistGroupExpanded(NewPlaylistGroupId);
        MarkAppStateDirty();
        RefreshPlaylistList(playlistListView);
        RefreshSelectedTrackList();
        return;
    }

    Playlist* selectedPlaylist = GetSelectedPlaylist();
    if (selectedPlaylist == nullptr)
    {
        return;
    }

    selectedPlaylist->tracks.insert(
        selectedPlaylist->tracks.end(),
        std::make_move_iterator(loadedPlaylist.tracks.begin()),
        std::make_move_iterator(loadedPlaylist.tracks.end()));
    selectedPlaylist->isModified = true;
    MarkAppStateDirty();
    RefreshSelectedTrackList();
}

bool AddAudioTrackFromPath(const std::wstring& path)
{
    Playlist* playlist = GetSelectedPlaylist();
    if (playlist == nullptr || !IsSupportedAudioPath(path))
    {
        return false;
    }
    AddTrack(*playlist, CreateTrackFromFile(path));
    return true;
}

std::vector<std::wstring> SelectAudioFiles(HWND window)
{
    constexpr DWORD FilePathBufferLength = 32768;
    std::vector<wchar_t> buffer(FilePathBufferLength, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = T(UiText::AudioFilesFilter);
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = T(UiText::OpenAudioFilesTitle);
    dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER |
                   OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(window, T(UiText::OpenDialogFailed),
                        WindowTitle, MB_OK | MB_ICONERROR);
        }
        return {};
    }

    const wchar_t* firstEntry = buffer.data();
    const wchar_t* nextEntry = firstEntry + std::wcslen(firstEntry) + 1;
    if (*nextEntry == L'\0')
    {
        return {firstEntry};
    }

    const std::filesystem::path directory(firstEntry);
    std::vector<std::wstring> paths;
    while (*nextEntry != L'\0')
    {
        paths.push_back((directory / nextEntry).lexically_normal().wstring());
        nextEntry += std::wcslen(nextEntry) + 1;
    }
    return paths;
}

void OpenAudioFiles(HWND window)
{
    if (!HasSelectedPlaylist())
    {
        MessageBoxW(window, T(UiText::CreatePlaylistFirst),
                    WindowTitle, MB_OK | MB_ICONINFORMATION);
        return;
    }

    const std::vector<std::wstring> paths = SelectAudioFiles(window);
    bool addedAudioTrack = false;
    for (const std::wstring& path : paths)
    {
        addedAudioTrack = AddAudioTrackFromPath(path) || addedAudioTrack;
    }
    if (addedAudioTrack)
    {
        MarkAppStateDirty();
        RefreshSelectedTrackList();
    }
}

void ImportPlaylistFromDialog(HWND window)
{
    constexpr DWORD FilePathBufferLength = 32768;
    std::vector<wchar_t> buffer(FilePathBufferLength, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = T(UiText::PlaylistFilesFilter);
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"m3u8";
    dialog.lpstrTitle = T(UiText::ImportPlaylistTitle);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(window, T(UiText::OpenDialogFailed),
                        WindowTitle, MB_OK | MB_ICONERROR);
        }
        return;
    }
    ImportPlaylistFile(window, buffer.data());
}

void HandleDroppedFiles(HWND window, HDROP drop)
{
    POINT dropPoint{};
    const bool droppedInTrackPane =
        DragQueryPoint(drop, &dropPoint) != FALSE &&
        IsPointInTrackPane(window, dropPoint);

    if (droppedInTrackPane)
    {
        bool addedAudioTrack = false;
        bool showedNoPlaylistMessage = false;
        const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT index = 0; index < fileCount; ++index)
        {
            const UINT length = DragQueryFileW(drop, index, nullptr, 0);
            std::vector<wchar_t> buffer(length + 1);
            DragQueryFileW(drop, index, buffer.data(),
                           static_cast<UINT>(buffer.size()));

            const std::wstring path(buffer.data());
            if (IsPlaylistPath(path))
            {
                ImportPlaylistFile(window, path);
                continue;
            }
            if (IsSupportedAudioPath(path))
            {
                if (AddAudioTrackFromPath(path))
                {
                    addedAudioTrack = true;
                }
                else if (!showedNoPlaylistMessage)
                {
                    MessageBoxW(window,
                                T(UiText::CreatePlaylistFirst),
                                WindowTitle, MB_OK | MB_ICONINFORMATION);
                    showedNoPlaylistMessage = true;
                }
            }
        }
        if (addedAudioTrack)
        {
            MarkAppStateDirty();
            RefreshSelectedTrackList();
        }
    }

    DragFinish(drop);
}

void RefreshOrganizerPlaylistList(
    const std::vector<int>& rowsToSelect = {})
{
    if (organizerPlaylistList == nullptr)
    {
        return;
    }
    ListView_DeleteAllItems(organizerPlaylistList);
    organizerVisiblePlaylistIndices.clear();
    if (selectedOrganizerGroupIndex < 0 ||
        selectedOrganizerGroupIndex >=
            static_cast<int>(playlistGroups.size()))
    {
        return;
    }

    const int groupId = playlistGroups[
        static_cast<std::size_t>(selectedOrganizerGroupIndex)].id;
    for (std::size_t playlistIndex = 0;
         playlistIndex < playlists.size(); ++playlistIndex)
    {
        if (playlists[playlistIndex].groupId != groupId)
        {
            continue;
        }
        organizerVisiblePlaylistIndices.push_back(
            static_cast<int>(playlistIndex));
        InsertListItem(organizerPlaylistList,
                       static_cast<int>(organizerVisiblePlaylistIndices.size()) - 1,
                       playlists[playlistIndex].name);
    }
    for (const int row : rowsToSelect)
    {
        if (row >= 0 &&
            row < static_cast<int>(organizerVisiblePlaylistIndices.size()))
        {
            ListView_SetItemState(organizerPlaylistList, row,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
}

void RefreshOrganizerGroupList()
{
    if (organizerGroupList == nullptr)
    {
        return;
    }
    selectedOrganizerGroupIndex = std::clamp(
        selectedOrganizerGroupIndex, 0,
        std::max(0, static_cast<int>(playlistGroups.size()) - 1));
    isRefreshingOrganizerGroups = true;
    ListView_DeleteAllItems(organizerGroupList);
    for (std::size_t index = 0; index < playlistGroups.size(); ++index)
    {
        InsertListItem(organizerGroupList, static_cast<int>(index),
                       playlistGroups[index].name);
    }
    if (!playlistGroups.empty())
    {
        ListView_SetItemState(organizerGroupList,
                              selectedOrganizerGroupIndex,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(organizerGroupList,
                               selectedOrganizerGroupIndex, FALSE);
    }
    isRefreshingOrganizerGroups = false;
    RefreshOrganizerPlaylistList();
}

void SelectOrganizerGroup(int groupIndex)
{
    if (groupIndex < 0 ||
        groupIndex >= static_cast<int>(playlistGroups.size()))
    {
        return;
    }
    selectedOrganizerGroupIndex = groupIndex;
    ListView_SetItemState(organizerGroupList, -1, 0,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(organizerGroupList, groupIndex,
                          LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    RefreshOrganizerPlaylistList();
}

void BeginOrganizerGroupRename()
{
    if (selectedOrganizerGroupIndex <= 0 ||
        selectedOrganizerGroupIndex >=
            static_cast<int>(playlistGroups.size()))
    {
        return;
    }
    SetFocus(organizerGroupList);
    ListView_EditLabel(organizerGroupList, selectedOrganizerGroupIndex);
}

void CreateOrganizerGroup()
{
    const std::wstring name = GenerateNewGroupName(playlistGroups);
    const int groupId = CreatePlaylistGroup(
        playlistGroups, nextGroupId, name);
    if (groupId < 0)
    {
        return;
    }
    selectedOrganizerGroupIndex =
        static_cast<int>(playlistGroups.size()) - 1;
    MarkAppStateDirty();
    RefreshOrganizerGroupList();
    BeginOrganizerGroupRename();
}

bool IsPlaylistGroupEmpty(int groupId)
{
    return std::none_of(
        playlists.begin(), playlists.end(),
        [groupId](const Playlist& playlist)
        {
            return playlist.groupId == groupId;
        });
}

bool CanDeletePlaylistGroup(int groupId)
{
    return groupId != NewPlaylistGroupId &&
        FindPlaylistGroupById(playlistGroups, groupId) != nullptr &&
        IsPlaylistGroupEmpty(groupId);
}

bool DeletePlaylistGroup(int groupId)
{
    if (!CanDeletePlaylistGroup(groupId))
    {
        return false;
    }

    const auto group = std::find_if(
        playlistGroups.begin(), playlistGroups.end(),
        [groupId](const PlaylistGroup& candidate)
        {
            return candidate.id == groupId;
        });
    if (group == playlistGroups.end())
    {
        return false;
    }

    const std::size_t erasedIndex = static_cast<std::size_t>(
        std::distance(playlistGroups.begin(), group));
    playlistGroups.erase(group);
    selectedOrganizerGroupIndex = static_cast<int>(std::min(
        erasedIndex, playlistGroups.size() - 1));
    MarkAppStateDirty();
    RefreshOrganizerGroupList();
    SetFocus(organizerGroupList);
    return true;
}

void ConfirmAndDeleteOrganizerGroup(HWND window, int groupId)
{
    if (!CanDeletePlaylistGroup(groupId))
    {
        return;
    }
    const PlaylistGroup* group =
        FindPlaylistGroupById(playlistGroups, groupId);
    if (group == nullptr)
    {
        return;
    }

    const std::wstring message = T(UiText::DeleteGroupQuestionPrefix) +
        group->name + T(UiText::DeleteGroupQuestionSuffix);
    if (MessageBoxW(window, message.c_str(),
                    T(UiText::PlaylistOrganizerTitle),
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
    {
        DeletePlaylistGroup(groupId);
    }
}

bool MoveOrganizerGroup(int direction)
{
    if ((direction != -1 && direction != 1) ||
        selectedOrganizerGroupIndex <= 0 ||
        selectedOrganizerGroupIndex >=
            static_cast<int>(playlistGroups.size()))
    {
        return false;
    }
    const int destination = selectedOrganizerGroupIndex + direction;
    if (destination <= 0 ||
        destination >= static_cast<int>(playlistGroups.size()))
    {
        return false;
    }
    std::swap(playlistGroups[
                  static_cast<std::size_t>(selectedOrganizerGroupIndex)],
              playlistGroups[static_cast<std::size_t>(destination)]);
    selectedOrganizerGroupIndex = destination;
    MarkAppStateDirty();
    RefreshOrganizerGroupList();
    SetFocus(organizerGroupList);
    return true;
}

std::vector<int> GetSelectedOrganizerPlaylistRows()
{
    std::vector<int> rows;
    int row = -1;
    while ((row = ListView_GetNextItem(
                organizerPlaylistList, row, LVNI_SELECTED)) != -1)
    {
        rows.push_back(row);
    }
    return rows;
}

std::vector<int> GetSelectedOrganizerPlaylistIndices()
{
    std::vector<int> playlistIndices;
    for (const int row : GetSelectedOrganizerPlaylistRows())
    {
        if (row >= 0 &&
            row < static_cast<int>(organizerVisiblePlaylistIndices.size()))
        {
            playlistIndices.push_back(
                organizerVisiblePlaylistIndices[static_cast<std::size_t>(row)]);
        }
    }
    return playlistIndices;
}

bool DeletePlaylistsByIndices(std::vector<int> playlistIndices)
{
    if (playlistIndices.empty())
    {
        return false;
    }
    std::sort(playlistIndices.begin(), playlistIndices.end());
    playlistIndices.erase(
        std::unique(playlistIndices.begin(), playlistIndices.end()),
        playlistIndices.end());
    if (playlistIndices.front() < 0 ||
        playlistIndices.back() >= static_cast<int>(playlists.size()))
    {
        return false;
    }

    std::vector<bool> isDeleted(playlists.size(), false);
    for (const int playlistIndex : playlistIndices)
    {
        isDeleted[static_cast<std::size_t>(playlistIndex)] = true;
    }

    const int previousSelectedPlaylistIndex = selectedPlaylistIndex;
    int newSelectedPlaylistIndex = -1;
    std::vector<Playlist> remaining;
    remaining.reserve(playlists.size() - playlistIndices.size());
    for (std::size_t oldIndex = 0; oldIndex < playlists.size(); ++oldIndex)
    {
        if (isDeleted[oldIndex])
        {
            continue;
        }
        if (static_cast<int>(oldIndex) == previousSelectedPlaylistIndex)
        {
            newSelectedPlaylistIndex = static_cast<int>(remaining.size());
        }
        remaining.push_back(std::move(playlists[oldIndex]));
    }

    playlists = std::move(remaining);
    selectedPlaylistIndex = newSelectedPlaylistIndex;
    MarkAppStateDirty();
    RefreshOrganizerPlaylistList();
    SetFocus(organizerPlaylistList);
    return true;
}

void ConfirmAndDeleteOrganizerPlaylists(HWND window)
{
    const std::vector<int> playlistIndices =
        GetSelectedOrganizerPlaylistIndices();
    if (playlistIndices.empty())
    {
        return;
    }

    std::wstring message;
    if (playlistIndices.size() == 1)
    {
        const int playlistIndex = playlistIndices.front();
        if (playlistIndex < 0 ||
            playlistIndex >= static_cast<int>(playlists.size()))
        {
            return;
        }
        message = T(UiText::DeletePlaylistQuestionPrefix) +
            playlists[static_cast<std::size_t>(playlistIndex)].name +
            T(UiText::DeletePlaylistQuestionSuffix);
    }
    else
    {
        message = std::wstring(T(UiText::DeletePlaylistsQuestionPrefix)) +
            std::to_wstring(playlistIndices.size()) +
            T(UiText::DeletePlaylistsQuestionSuffix);
    }
    message += L"\n\n";
    message += T(UiText::AudioFilesNotDeleted);

    if (MessageBoxW(window, message.c_str(),
                    T(UiText::PlaylistOrganizerTitle),
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)
    {
        DeletePlaylistsByIndices(playlistIndices);
    }
}

bool MoveOrganizerPlaylistsToGroup(
    const std::vector<int>& selectedIndices, int targetGroupId)
{
    if (FindPlaylistGroupById(playlistGroups, targetGroupId) == nullptr ||
        selectedOrganizerGroupIndex < 0 ||
        selectedOrganizerGroupIndex >=
            static_cast<int>(playlistGroups.size()) ||
        playlistGroups[static_cast<std::size_t>(
            selectedOrganizerGroupIndex)].id == targetGroupId)
    {
        return false;
    }

    if (selectedIndices.empty())
    {
        return false;
    }

    std::vector<bool> isSelected(playlists.size(), false);
    for (const int playlistIndex : selectedIndices)
    {
        if (playlistIndex < 0 ||
            playlistIndex >= static_cast<int>(playlists.size()))
        {
            return false;
        }
        isSelected[static_cast<std::size_t>(playlistIndex)] = true;
    }

    struct IndexedPlaylist
    {
        int originalIndex = -1;
        Playlist playlist;
    };

    std::vector<IndexedPlaylist> remaining;
    std::vector<IndexedPlaylist> moving;
    remaining.reserve(playlists.size() - selectedIndices.size());
    moving.reserve(selectedIndices.size());
    for (std::size_t index = 0; index < playlists.size(); ++index)
    {
        IndexedPlaylist item{
            static_cast<int>(index), std::move(playlists[index])};
        if (isSelected[index])
        {
            item.playlist.groupId = targetGroupId;
            moving.push_back(std::move(item));
        }
        else
        {
            remaining.push_back(std::move(item));
        }
    }

    std::size_t insertionIndex = remaining.size();
    bool foundTargetPlaylist = false;
    for (std::size_t index = 0; index < remaining.size(); ++index)
    {
        if (remaining[index].playlist.groupId == targetGroupId)
        {
            insertionIndex = index + 1;
            foundTargetPlaylist = true;
        }
    }
    if (!foundTargetPlaylist)
    {
        insertionIndex = remaining.size();
    }

    const int previousSelectedPlaylistIndex = selectedPlaylistIndex;
    int newSelectedPlaylistIndex = -1;
    std::vector<Playlist> reordered;
    reordered.reserve(playlists.size());
    auto appendPlaylist = [&](IndexedPlaylist& item)
    {
        if (item.originalIndex == previousSelectedPlaylistIndex)
        {
            newSelectedPlaylistIndex = static_cast<int>(reordered.size());
        }
        reordered.push_back(std::move(item.playlist));
    };

    for (std::size_t index = 0; index < insertionIndex; ++index)
    {
        appendPlaylist(remaining[index]);
    }
    for (IndexedPlaylist& item : moving)
    {
        appendPlaylist(item);
    }
    for (std::size_t index = insertionIndex; index < remaining.size(); ++index)
    {
        appendPlaylist(remaining[index]);
    }

    playlists = std::move(reordered);
    selectedPlaylistIndex = newSelectedPlaylistIndex;
    MarkAppStateDirty();
    RefreshOrganizerPlaylistList();
    SetFocus(organizerPlaylistList);
    return true;
}

bool MoveSelectedOrganizerPlaylistsToGroup(int targetGroupId)
{
    return MoveOrganizerPlaylistsToGroup(
        GetSelectedOrganizerPlaylistIndices(), targetGroupId);
}

bool MoveOrganizerPlaylists(int direction)
{
    const std::vector<int> selectedRows =
        GetSelectedOrganizerPlaylistRows();
    if ((direction != -1 && direction != 1) ||
        !AreIndicesContiguous(selectedRows))
    {
        return false;
    }
    const int first = selectedRows.front();
    const int last = selectedRows.back();
    const int count =
        static_cast<int>(organizerVisiblePlaylistIndices.size());
    if ((direction < 0 && first == 0) ||
        (direction > 0 && last == count - 1) ||
        first < 0 || last >= count)
    {
        return false;
    }

    std::vector<Playlist> groupPlaylists;
    groupPlaylists.reserve(organizerVisiblePlaylistIndices.size());
    int selectedMainOrdinal = -1;
    for (std::size_t ordinal = 0;
         ordinal < organizerVisiblePlaylistIndices.size(); ++ordinal)
    {
        const int playlistIndex = organizerVisiblePlaylistIndices[ordinal];
        if (playlistIndex == selectedPlaylistIndex)
        {
            selectedMainOrdinal = static_cast<int>(ordinal);
        }
        groupPlaylists.push_back(std::move(
            playlists[static_cast<std::size_t>(playlistIndex)]));
    }

    if (direction < 0)
    {
        std::rotate(groupPlaylists.begin() + first - 1,
                    groupPlaylists.begin() + first,
                    groupPlaylists.begin() + last + 1);
        if (selectedMainOrdinal >= first && selectedMainOrdinal <= last)
            --selectedMainOrdinal;
        else if (selectedMainOrdinal == first - 1)
            selectedMainOrdinal = last;
    }
    else
    {
        std::rotate(groupPlaylists.begin() + first,
                    groupPlaylists.begin() + last + 1,
                    groupPlaylists.begin() + last + 2);
        if (selectedMainOrdinal >= first && selectedMainOrdinal <= last)
            ++selectedMainOrdinal;
        else if (selectedMainOrdinal == last + 1)
            selectedMainOrdinal = first;
    }

    for (std::size_t ordinal = 0;
         ordinal < organizerVisiblePlaylistIndices.size(); ++ordinal)
    {
        playlists[static_cast<std::size_t>(
            organizerVisiblePlaylistIndices[ordinal])] =
                std::move(groupPlaylists[ordinal]);
    }
    if (selectedMainOrdinal >= 0)
    {
        selectedPlaylistIndex = organizerVisiblePlaylistIndices[
            static_cast<std::size_t>(selectedMainOrdinal)];
    }

    std::vector<int> movedRows = selectedRows;
    for (int& row : movedRows)
    {
        row += direction;
    }
    MarkAppStateDirty();
    RefreshOrganizerPlaylistList(movedRows);
    SetFocus(organizerPlaylistList);
    return true;
}

void ShowOrganizerGroupContextMenu(HWND window, LPARAM lParam)
{
    POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    organizerContextGroupId = -1;
    if (screenPoint.x != -1 || screenPoint.y != -1)
    {
        POINT clientPoint = screenPoint;
        ScreenToClient(organizerGroupList, &clientPoint);
        LVHITTESTINFO hitTest{};
        hitTest.pt = clientPoint;
        const int row = ListView_HitTest(organizerGroupList, &hitTest);
        if (row >= 0)
        {
            SelectOrganizerGroup(row);
            organizerContextGroupId = playlistGroups[
                static_cast<std::size_t>(row)].id;
        }
    }
    else
    {
        RECT itemRect{};
        if (ListView_GetItemRect(organizerGroupList,
                                 selectedOrganizerGroupIndex,
                                 &itemRect, LVIR_BOUNDS))
        {
            screenPoint = {itemRect.left, itemRect.bottom};
            ClientToScreen(organizerGroupList, &screenPoint);
            organizerContextGroupId = playlistGroups[
                static_cast<std::size_t>(selectedOrganizerGroupIndex)].id;
        }
    }
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        GetCursorPos(&screenPoint);
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }
    AppendMenuW(menu, MF_STRING, OrganizerCommandNewGroup,
                T(UiText::NewGroup));
    const bool canRename = organizerContextGroupId != -1 &&
        organizerContextGroupId != NewPlaylistGroupId;
    AppendMenuW(menu,
                MF_STRING | (!canRename
                    ? MF_GRAYED : MF_ENABLED),
                OrganizerCommandRenameGroup, T(UiText::RenameGroup));
    AppendMenuW(menu,
                MF_STRING | (CanDeletePlaylistGroup(organizerContextGroupId)
                    ? MF_ENABLED : MF_GRAYED),
                OrganizerCommandDeleteGroup, T(UiText::DeleteGroup));
    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command != 0)
    {
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

void ShowOrganizerPlaylistContextMenu(HWND window, LPARAM lParam)
{
    POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (screenPoint.x != -1 || screenPoint.y != -1)
    {
        POINT clientPoint = screenPoint;
        ScreenToClient(organizerPlaylistList, &clientPoint);
        LVHITTESTINFO hitTest{};
        hitTest.pt = clientPoint;
        const int row = ListView_HitTest(organizerPlaylistList, &hitTest);
        if (row < 0)
        {
            return;
        }
        if ((ListView_GetItemState(organizerPlaylistList, row,
                                   LVIS_SELECTED) & LVIS_SELECTED) == 0)
        {
            ListView_SetItemState(organizerPlaylistList, -1, 0,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(organizerPlaylistList, row,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    else
    {
        const int row = ListView_GetNextItem(
            organizerPlaylistList, -1, LVNI_SELECTED);
        if (row < 0)
        {
            return;
        }
        RECT itemRect{};
        if (ListView_GetItemRect(organizerPlaylistList, row,
                                 &itemRect, LVIR_BOUNDS))
        {
            screenPoint = {itemRect.left, itemRect.bottom};
            ClientToScreen(organizerPlaylistList, &screenPoint);
        }
    }
    if (GetSelectedOrganizerPlaylistRows().empty())
    {
        return;
    }

    SetFocus(organizerPlaylistList);
    HMENU menu = CreatePopupMenu();
    HMENU moveMenu = CreatePopupMenu();
    if (menu == nullptr || moveMenu == nullptr)
    {
        if (moveMenu != nullptr)
        {
            DestroyMenu(moveMenu);
        }
        if (menu != nullptr)
        {
            DestroyMenu(menu);
        }
        return;
    }

    organizerMoveMenuGroupIds.clear();
    organizerMoveMenuGroupIds.reserve(playlistGroups.size());
    const int currentGroupId =
        selectedOrganizerGroupIndex >= 0 &&
        selectedOrganizerGroupIndex < static_cast<int>(playlistGroups.size())
            ? playlistGroups[static_cast<std::size_t>(
                  selectedOrganizerGroupIndex)].id
            : -1;
    for (const PlaylistGroup& group : playlistGroups)
    {
        const UINT command = OrganizerMoveToGroupCommandBase +
            static_cast<UINT>(organizerMoveMenuGroupIds.size());
        organizerMoveMenuGroupIds.push_back(group.id);
        AppendMenuW(moveMenu,
                    MF_STRING | (group.id == currentGroupId
                        ? MF_GRAYED : MF_ENABLED),
                    command, group.name.c_str());
    }
    AppendMenuW(menu, MF_POPUP,
                reinterpret_cast<UINT_PTR>(moveMenu), T(UiText::MoveTo));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, OrganizerCommandDeletePlaylists,
                T(UiText::DeletePlaylist));

    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command != 0)
    {
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }
}

void LayoutOrganizerChildren(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    constexpr int margin = 12;
    constexpr int labelHeight = 20;
    constexpr int buttonHeight = 30;
    constexpr int gap = 10;
    const int width = std::max(
        0, static_cast<int>(client.right - client.left));
    const int height = std::max(
        0, static_cast<int>(client.bottom - client.top));
    const int leftWidth = std::max(160, (width - margin * 2 - gap) / 3);
    const int rightX = margin + leftWidth + gap;
    const int rightWidth = std::max(0, width - rightX - margin);
    const int listTop = margin + labelHeight;
    const int listHeight = std::max(
        0, height - listTop - buttonHeight - margin * 2);
    MoveWindow(GetDlgItem(window, 2201), margin, margin,
               leftWidth, labelHeight, TRUE);
    MoveWindow(GetDlgItem(window, 2202), rightX, margin,
               rightWidth, labelHeight, TRUE);
    MoveWindow(organizerGroupList, margin, listTop,
               leftWidth, listHeight, TRUE);
    MoveWindow(organizerPlaylistList, rightX, listTop,
               rightWidth, listHeight, TRUE);
    MoveWindow(organizerNewGroupButton, margin,
               height - margin - buttonHeight, 110, buttonHeight, TRUE);
    MoveWindow(organizerCloseButton,
               width - margin - 90, height - margin - buttonHeight,
               90, buttonHeight, TRUE);
    ListView_SetColumnWidth(organizerGroupList, 0,
                            std::max(0, leftWidth - 4));
    ListView_SetColumnWidth(organizerPlaylistList, 0,
                            std::max(0, rightWidth - 4));
}

LRESULT CALLBACK PlaylistOrganizerWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(
            lParam)->hInstance;
        CreateWindowExW(0, WC_STATICW, T(UiText::GroupsLabel),
                        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                        window, reinterpret_cast<HMENU>(2201), instance,
                        nullptr);
        CreateWindowExW(0, WC_STATICW, T(UiText::PlaylistsLabel),
                        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                        window, reinterpret_cast<HMENU>(2202), instance,
                        nullptr);
        organizerGroupList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
                LVS_SHOWSELALWAYS | LVS_EDITLABELS,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(OrganizerGroupListId), instance, nullptr);
        organizerPlaylistList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(OrganizerPlaylistListId), instance,
            nullptr);
        organizerNewGroupButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::NewGroup),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(OrganizerNewGroupButtonId), instance,
            nullptr);
        organizerCloseButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Close),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(OrganizerCloseButtonId), instance,
            nullptr);
        if (organizerGroupList == nullptr ||
            organizerPlaylistList == nullptr ||
            organizerNewGroupButton == nullptr ||
            organizerCloseButton == nullptr)
        {
            return -1;
        }
        ListView_SetExtendedListViewStyle(
            organizerGroupList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetExtendedListViewStyle(
            organizerPlaylistList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        InsertColumn(organizerGroupList, 0, T(UiText::GroupHeader), 220);
        InsertColumn(organizerPlaylistList, 0, T(UiText::PlaylistHeader), 480);
        RefreshOrganizerGroupList();
        return 0;
    }
    case WM_SIZE:
        LayoutOrganizerChildren(window);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) >= OrganizerMoveToGroupCommandBase)
        {
            const std::size_t mappingIndex =
                LOWORD(wParam) - OrganizerMoveToGroupCommandBase;
            if (mappingIndex < organizerMoveMenuGroupIds.size())
            {
                MoveSelectedOrganizerPlaylistsToGroup(
                    organizerMoveMenuGroupIds[mappingIndex]);
                return 0;
            }
        }
        switch (LOWORD(wParam))
        {
        case OrganizerNewGroupButtonId:
        case OrganizerCommandNewGroup:
            CreateOrganizerGroup();
            return 0;
        case OrganizerCommandRenameGroup:
            BeginOrganizerGroupRename();
            return 0;
        case OrganizerCommandDeleteGroup:
            ConfirmAndDeleteOrganizerGroup(window, organizerContextGroupId);
            return 0;
        case OrganizerCommandDeletePlaylists:
            ConfirmAndDeleteOrganizerPlaylists(window);
            return 0;
        case OrganizerCloseButtonId:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_NOTIFY:
    {
        NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->hwndFrom != organizerGroupList)
        {
            break;
        }
        if (header->code == LVN_ITEMCHANGED &&
            !isRefreshingOrganizerGroups)
        {
            const NMLISTVIEW* change =
                reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((change->uChanged & LVIF_STATE) != 0 &&
                (change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0 &&
                change->iItem >= 0 &&
                change->iItem < static_cast<int>(playlistGroups.size()))
            {
                selectedOrganizerGroupIndex = change->iItem;
                RefreshOrganizerPlaylistList();
            }
            return 0;
        }
        if (header->code == LVN_BEGINLABELEDITW)
        {
            const NMLVDISPINFOW* edit =
                reinterpret_cast<NMLVDISPINFOW*>(lParam);
            return edit->item.iItem == 0 ? TRUE : FALSE;
        }
        if (header->code == LVN_ENDLABELEDITW)
        {
            const NMLVDISPINFOW* edit =
                reinterpret_cast<NMLVDISPINFOW*>(lParam);
            const int groupIndex = edit->item.iItem;
            if (groupIndex <= 0 ||
                groupIndex >= static_cast<int>(playlistGroups.size()) ||
                edit->item.pszText == nullptr)
            {
                return FALSE;
            }
            std::vector<PlaylistGroup> otherGroups = playlistGroups;
            otherGroups.erase(otherGroups.begin() + groupIndex);
            if (!IsPlaylistGroupNameAvailable(otherGroups,
                                               edit->item.pszText))
            {
                MessageBoxW(window,
                            T(UiText::GroupNamesUnique),
                            T(UiText::PlaylistOrganizerTitle),
                            MB_OK | MB_ICONINFORMATION);
                return FALSE;
            }
            playlistGroups[static_cast<std::size_t>(groupIndex)].name =
                edit->item.pszText;
            MarkAppStateDirty();
            return TRUE;
        }
        if (header->code == LVN_KEYDOWN)
        {
            const NMLVKEYDOWN* key =
                reinterpret_cast<NMLVKEYDOWN*>(lParam);
            if (key->wVKey == VK_F2)
            {
                BeginOrganizerGroupRename();
            }
            return 0;
        }
        break;
    }
    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(wParam) == organizerGroupList)
        {
            ShowOrganizerGroupContextMenu(window, lParam);
            return 0;
        }
        if (reinterpret_cast<HWND>(wParam) == organizerPlaylistList)
        {
            ShowOrganizerPlaylistContextMenu(window, lParam);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        organizerWindow = nullptr;
        organizerGroupList = nullptr;
        organizerPlaylistList = nullptr;
        organizerNewGroupButton = nullptr;
        organizerCloseButton = nullptr;
        EnableWindow(mainWindow, TRUE);
        RefreshPlaylistList(playlistListView);
        RefreshSelectedTrackList();
        SetForegroundWindow(mainWindow);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowPlaylistOrganizer(HWND owner)
{
    if (!SaveAppState(CaptureCurrentAppState()))
    {
        MessageBoxW(owner,
                    T(UiText::StateSaveFailed),
                    WindowTitle, MB_OK | MB_ICONWARNING);
    }
    else
    {
        appStateDirty = false;
    }

    selectedOrganizerGroupIndex = 0;
    if (const Playlist* selectedPlaylist = GetSelectedPlaylist())
    {
        for (std::size_t index = 0; index < playlistGroups.size(); ++index)
        {
            if (playlistGroups[index].id == selectedPlaylist->groupId)
            {
                selectedOrganizerGroupIndex = static_cast<int>(index);
                break;
            }
        }
    }

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    constexpr int width = 800;
    constexpr int height = 500;
    const int x = ownerRect.left +
                  ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top +
                  ((ownerRect.bottom - ownerRect.top) - height) / 2;
    organizerWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        OrganizerWindowClassName, T(UiText::PlaylistOrganizerTitle),
        WS_OVERLAPPEDWINDOW, x, y, width, height,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (organizerWindow == nullptr)
    {
        return;
    }
    EnableWindow(owner, FALSE);
    ShowWindow(organizerWindow, SW_SHOW);
    UpdateWindow(organizerWindow);

    MSG message{};
    while (IsWindow(organizerWindow) && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        const bool altArrow =
            (message.message == WM_KEYDOWN ||
             message.message == WM_SYSKEYDOWN) &&
            (message.wParam == VK_UP || message.wParam == VK_DOWN) &&
            (GetKeyState(VK_MENU) & 0x8000) != 0 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
            (GetKeyState(VK_SHIFT) & 0x8000) == 0;
        if (altArrow)
        {
            const int direction = message.wParam == VK_UP ? -1 : 1;
            const HWND focus = GetFocus();
            if (focus == organizerGroupList)
            {
                MoveOrganizerGroup(direction);
                continue;
            }
            if (focus == organizerPlaylistList)
            {
                MoveOrganizerPlaylists(direction);
                continue;
            }
        }
        if (!IsDialogMessageW(organizerWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

std::wstring GetControlText(HWND control)
{
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(std::max(0, length)) + 1,
                      L'\0');
    if (length > 0)
    {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<std::size_t>(std::max(0, length)));
    return text;
}

bool HasNonWhitespaceText(const std::wstring& text)
{
    return std::any_of(text.begin(), text.end(), [](wchar_t character) {
        return std::iswspace(character) == 0;
    });
}

bool IsSendToNameAvailable(const std::wstring& name, int ignoreIndex = -1)
{
    if (!HasNonWhitespaceText(name))
    {
        return false;
    }
    for (std::size_t index = 0; index < sendToApplications.size(); ++index)
    {
        if (static_cast<int>(index) != ignoreIndex &&
            CompareStringOrdinal(name.c_str(), -1,
                                 sendToApplications[index].name.c_str(), -1,
                                 TRUE) == CSTR_EQUAL)
        {
            return false;
        }
    }
    return true;
}

std::size_t CountSendToPlaceholder(const std::wstring& arguments,
                                   const std::wstring& placeholder)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = arguments.find(placeholder, position)) !=
           std::wstring::npos)
    {
        ++count;
        position += placeholder.size();
    }
    return count;
}

SendToArgumentMode GetSendToArgumentMode(const std::wstring& arguments)
{
    if (!HasNonWhitespaceText(arguments))
    {
        return SendToArgumentMode::Invalid;
    }
    const std::size_t filesCount =
        CountSendToPlaceholder(arguments, L"%files%");
    const std::size_t folderCount =
        CountSendToPlaceholder(arguments, L"%folder%");
    if (filesCount == 1 && folderCount == 0)
    {
        return SendToArgumentMode::Files;
    }
    if (filesCount == 0 && folderCount == 1)
    {
        return SendToArgumentMode::Folder;
    }
    return SendToArgumentMode::Invalid;
}

bool IsValidSendToArguments(const std::wstring& arguments)
{
    return GetSendToArgumentMode(arguments) != SendToArgumentMode::Invalid;
}

bool CanUseSendToApplication(const SendToApplication& application,
                             int selectedTrackCount)
{
    switch (GetSendToArgumentMode(application.arguments))
    {
    case SendToArgumentMode::Files:
        return selectedTrackCount >= 1;
    case SendToArgumentMode::Folder:
        return selectedTrackCount == 1;
    default:
        return false;
    }
}

bool ValidateSendToApplication(HWND owner,
                               const SendToApplication& application,
                               int ignoreIndex)
{
    if (!HasNonWhitespaceText(application.name))
    {
        MessageBoxW(owner, T(UiText::NameRequired),
                    T(UiText::SendToWindowTitle),
                    MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!IsSendToNameAvailable(application.name, ignoreIndex))
    {
        MessageBoxW(owner, T(UiText::ApplicationNamesUnique),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!HasNonWhitespaceText(application.executablePath))
    {
        MessageBoxW(owner, T(UiText::ExecutableRequired),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(application.executablePath, error) ||
        error)
    {
        MessageBoxW(owner, T(UiText::ExecutableMustExist),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!HasNonWhitespaceText(application.arguments))
    {
        MessageBoxW(owner, T(UiText::ArgumentsRequired),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!IsValidSendToArguments(application.arguments))
    {
        MessageBoxW(owner,
                    T(UiText::ArgumentsPlaceholderRequired),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (ignoreIndex < 0 &&
        sendToApplications.size() >= MaximumSendToApplications)
    {
        MessageBoxW(owner, T(UiText::ApplicationLimit),
                    T(UiText::SendToWindowTitle), MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

int GetSelectedSendToApplicationIndex()
{
    return sendToList == nullptr
        ? -1
        : ListView_GetNextItem(sendToList, -1, LVNI_SELECTED);
}

void UpdateSendToSettingsButtons()
{
    const int selected = GetSelectedSendToApplicationIndex();
    const bool hasSelection = selected >= 0;
    EnableWindow(sendToAddButton,
                 sendToApplications.size() < MaximumSendToApplications);
    EnableWindow(sendToEditButton, hasSelection);
    EnableWindow(sendToRemoveButton, hasSelection);
    EnableWindow(sendToMoveUpButton, selected > 0);
    EnableWindow(sendToMoveDownButton,
                 selected >= 0 &&
                 selected + 1 < static_cast<int>(sendToApplications.size()));
}

void RefreshSendToApplicationsList(int rowToSelect = -1)
{
    if (sendToList == nullptr)
    {
        return;
    }
    ListView_DeleteAllItems(sendToList);
    for (std::size_t index = 0; index < sendToApplications.size(); ++index)
    {
        InsertListItem(sendToList, static_cast<int>(index),
                       sendToApplications[index].name);
        ListView_SetItemText(
            sendToList, static_cast<int>(index), 1,
            const_cast<wchar_t*>(
                sendToApplications[index].executablePath.c_str()));
    }
    if (rowToSelect >= 0 &&
        rowToSelect < static_cast<int>(sendToApplications.size()))
    {
        ListView_SetItemState(sendToList, rowToSelect,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(sendToList, rowToSelect, FALSE);
    }
    UpdateSendToSettingsButtons();
}

void BrowseForSendToExecutable(HWND owner)
{
    std::array<wchar_t, 32768> path{};
    const std::wstring current = GetControlText(sendToEditorExecutable);
    if (current.size() < path.size())
    {
        std::copy(current.begin(), current.end(), path.begin());
    }
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = T(UiText::ApplicationsFilter);
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_HIDEREADONLY;
    dialog.lpstrDefExt = L"exe";
    dialog.lpstrTitle = T(UiText::SelectExecutableTitle);
    if (GetOpenFileNameW(&dialog))
    {
        SetWindowTextW(sendToEditorExecutable, path.data());
    }
}

void LayoutSendToEditor(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    constexpr int margin = 14;
    constexpr int labelWidth = 82;
    constexpr int rowHeight = 25;
    constexpr int gap = 12;
    constexpr int browseWidth = 88;
    const int width = client.right - client.left;
    const int editX = margin + labelWidth;
    const int editWidth = std::max(80, width - editX - margin);
    MoveWindow(GetDlgItem(window, 3201), margin, margin,
               labelWidth, rowHeight, TRUE);
    MoveWindow(sendToEditorName, editX, margin,
               editWidth, rowHeight, TRUE);
    const int executableY = margin + rowHeight + gap;
    MoveWindow(GetDlgItem(window, 3202), margin, executableY,
               labelWidth, rowHeight, TRUE);
    MoveWindow(sendToEditorExecutable, editX, executableY,
               std::max(40, editWidth - browseWidth - 6), rowHeight, TRUE);
    MoveWindow(GetDlgItem(window, SendToEditorBrowseId),
               editX + editWidth - browseWidth, executableY,
               browseWidth, rowHeight, TRUE);
    const int argumentsY = executableY + rowHeight + gap;
    MoveWindow(GetDlgItem(window, 3203), margin, argumentsY,
               labelWidth, rowHeight, TRUE);
    MoveWindow(sendToEditorArguments, editX, argumentsY,
               editWidth, rowHeight, TRUE);
    const int buttonY = client.bottom - margin - 28;
    MoveWindow(GetDlgItem(window, SendToEditorOkId),
               width - margin - 166, buttonY, 78, 28, TRUE);
    MoveWindow(GetDlgItem(window, SendToEditorCancelId),
               width - margin - 82, buttonY, 82, 28, TRUE);
}

LRESULT CALLBACK SendToEditorWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(
            lParam)->hInstance;
        const std::wstring nameLabel = std::wstring(T(UiText::Name)) + L":";
        const std::wstring executableLabel =
            std::wstring(T(UiText::Executable)) + L":";
        const std::wstring argumentsLabel =
            std::wstring(T(UiText::Arguments)) + L":";
        CreateWindowExW(0, WC_STATICW, nameLabel.c_str(),
                        WS_CHILD | WS_VISIBLE,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(3201), instance, nullptr);
        CreateWindowExW(0, WC_STATICW, executableLabel.c_str(),
                        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(3202), instance, nullptr);
        CreateWindowExW(0, WC_STATICW, argumentsLabel.c_str(),
                        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(3203), instance, nullptr);
        sendToEditorName = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToEditorNameId), instance, nullptr);
        sendToEditorExecutable = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToEditorExecutableId), instance,
            nullptr);
        sendToEditorArguments = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToEditorArgumentsId), instance,
            nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Browse),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(SendToEditorBrowseId),
                        instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Ok),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(SendToEditorOkId), instance,
                        nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Cancel),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        0, 0, 0, 0, window,
                        reinterpret_cast<HMENU>(SendToEditorCancelId),
                        instance, nullptr);
        if (sendToEditorIndex >= 0 &&
            sendToEditorIndex < static_cast<int>(sendToApplications.size()))
        {
            const SendToApplication& application =
                sendToApplications[static_cast<std::size_t>(sendToEditorIndex)];
            SetWindowTextW(sendToEditorName, application.name.c_str());
            SetWindowTextW(sendToEditorExecutable,
                           application.executablePath.c_str());
            SetWindowTextW(sendToEditorArguments,
                           application.arguments.c_str());
        }
        SetFocus(sendToEditorName);
        return 0;
    }
    case WM_SIZE:
        LayoutSendToEditor(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case SendToEditorBrowseId:
            BrowseForSendToExecutable(window);
            return 0;
        case SendToEditorOkId:
        {
            SendToApplication application{
                GetControlText(sendToEditorName),
                GetControlText(sendToEditorExecutable),
                GetControlText(sendToEditorArguments)};
            if (!ValidateSendToApplication(
                    window, application, sendToEditorIndex))
            {
                return 0;
            }
            int selectedRow = sendToEditorIndex;
            if (sendToEditorIndex >= 0)
            {
                sendToApplications[static_cast<std::size_t>(
                    sendToEditorIndex)] = std::move(application);
            }
            else
            {
                sendToApplications.push_back(std::move(application));
                selectedRow = static_cast<int>(sendToApplications.size()) - 1;
            }
            MarkAppStateDirty();
            RefreshSendToApplicationsList(selectedRow);
            DestroyWindow(window);
            return 0;
        }
        case SendToEditorCancelId:
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        sendToEditorWindow = nullptr;
        sendToEditorName = nullptr;
        sendToEditorExecutable = nullptr;
        sendToEditorArguments = nullptr;
        EnableWindow(sendToSettingsWindow, TRUE);
        SetForegroundWindow(sendToSettingsWindow);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowSendToApplicationEditor(int applicationIndex)
{
    if (applicationIndex < -1 ||
        applicationIndex >= static_cast<int>(sendToApplications.size()) ||
        (applicationIndex < 0 &&
         sendToApplications.size() >= MaximumSendToApplications))
    {
        return;
    }
    sendToEditorIndex = applicationIndex;
    RECT ownerRect{};
    GetWindowRect(sendToSettingsWindow, &ownerRect);
    constexpr int width = 650;
    constexpr int height = 210;
    sendToEditorWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        SendToEditorWindowClassName,
        applicationIndex < 0 ? T(UiText::AddSendToApplication)
                             : T(UiText::EditSendToApplication),
        WS_CAPTION | WS_SYSMENU | WS_SIZEBOX,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
        width, height, sendToSettingsWindow, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (sendToEditorWindow == nullptr)
    {
        return;
    }
    EnableWindow(sendToSettingsWindow, FALSE);
    ShowWindow(sendToEditorWindow, SW_SHOW);
    UpdateWindow(sendToEditorWindow);
    MSG message{};
    while (IsWindow(sendToEditorWindow) &&
           GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(sendToEditorWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

void RemoveSelectedSendToApplication(HWND owner)
{
    const int index = GetSelectedSendToApplicationIndex();
    if (index < 0 || index >= static_cast<int>(sendToApplications.size()))
    {
        return;
    }
    const std::wstring message = T(UiText::RemoveSendToPrefix) +
        sendToApplications[static_cast<std::size_t>(index)].name +
        T(UiText::RemoveSendToSuffix);
    if (MessageBoxW(owner, message.c_str(), T(UiText::SendToWindowTitle),
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }
    sendToApplications.erase(sendToApplications.begin() + index);
    MarkAppStateDirty();
    const int selectedRow = sendToApplications.empty()
        ? -1
        : std::min(index, static_cast<int>(sendToApplications.size()) - 1);
    RefreshSendToApplicationsList(selectedRow);
}

void MoveSelectedSendToApplication(int direction)
{
    const int selected = GetSelectedSendToApplicationIndex();
    const int destination = selected + direction;
    if ((direction != -1 && direction != 1) || selected < 0 ||
        destination < 0 ||
        destination >= static_cast<int>(sendToApplications.size()))
    {
        return;
    }
    std::swap(sendToApplications[static_cast<std::size_t>(selected)],
              sendToApplications[static_cast<std::size_t>(destination)]);
    MarkAppStateDirty();
    RefreshSendToApplicationsList(destination);
}

void LayoutSendToSettings(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    constexpr int margin = 12;
    constexpr int buttonWidth = 90;
    constexpr int buttonHeight = 30;
    constexpr int gap = 8;
    constexpr int moveButtonWidth = 48;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int buttonY = height - margin - buttonHeight;
    MoveWindow(sendToList, margin, margin, width - margin * 2,
               std::max(0, buttonY - margin - gap), TRUE);
    MoveWindow(sendToAddButton, margin, buttonY,
               buttonWidth, buttonHeight, TRUE);
    MoveWindow(sendToEditButton, margin + buttonWidth + gap, buttonY,
               buttonWidth, buttonHeight, TRUE);
    MoveWindow(sendToRemoveButton, margin + (buttonWidth + gap) * 2, buttonY,
               buttonWidth, buttonHeight, TRUE);
    const int moveDownX = width - margin - buttonWidth - gap -
        moveButtonWidth;
    const int moveUpX = moveDownX - gap - moveButtonWidth;
    MoveWindow(sendToMoveUpButton, moveUpX, buttonY,
               moveButtonWidth, buttonHeight, TRUE);
    MoveWindow(sendToMoveDownButton, moveDownX, buttonY,
               moveButtonWidth, buttonHeight, TRUE);
    MoveWindow(sendToCloseButton, width - margin - buttonWidth, buttonY,
               buttonWidth, buttonHeight, TRUE);
    ListView_SetColumnWidth(sendToList, 0, std::max(100, width / 3));
    ListView_SetColumnWidth(sendToList, 1,
                            std::max(100, width - width / 3 - margin * 2 - 5));
}

LRESULT CALLBACK SendToSettingsWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(
            lParam)->hInstance;
        sendToList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(SendToListId),
            instance, nullptr);
        sendToAddButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Add),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToAddButtonId), instance, nullptr);
        sendToEditButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Edit),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToEditButtonId), instance, nullptr);
        sendToRemoveButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Remove),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToRemoveButtonId), instance, nullptr);
        sendToMoveUpButton = CreateWindowExW(
            0, WC_BUTTONW, L"\u2191", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToMoveUpButtonId), instance, nullptr);
        sendToMoveDownButton = CreateWindowExW(
            0, WC_BUTTONW, L"\u2193", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToMoveDownButtonId), instance,
            nullptr);
        sendToCloseButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Close),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(SendToCloseButtonId), instance, nullptr);
        if (sendToList == nullptr || sendToAddButton == nullptr ||
            sendToEditButton == nullptr || sendToRemoveButton == nullptr ||
            sendToMoveUpButton == nullptr ||
            sendToMoveDownButton == nullptr ||
            sendToCloseButton == nullptr)
        {
            return -1;
        }
        ListView_SetExtendedListViewStyle(
            sendToList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        InsertColumn(sendToList, 0, T(UiText::Name), 220);
        InsertColumn(sendToList, 1, T(UiText::Executable), 480);
        RefreshSendToApplicationsList();
        return 0;
    }
    case WM_SIZE:
        LayoutSendToSettings(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case SendToAddButtonId:
            ShowSendToApplicationEditor(-1);
            return 0;
        case SendToEditButtonId:
            ShowSendToApplicationEditor(GetSelectedSendToApplicationIndex());
            return 0;
        case SendToRemoveButtonId:
            RemoveSelectedSendToApplication(window);
            return 0;
        case SendToMoveUpButtonId:
            MoveSelectedSendToApplication(-1);
            return 0;
        case SendToMoveDownButtonId:
            MoveSelectedSendToApplication(1);
            return 0;
        case SendToCloseButtonId:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_NOTIFY:
    {
        NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->hwndFrom == sendToList)
        {
            if (header->code == LVN_ITEMCHANGED)
            {
                UpdateSendToSettingsButtons();
                return 0;
            }
            if (header->code == NM_DBLCLK)
            {
                ShowSendToApplicationEditor(
                    GetSelectedSendToApplicationIndex());
                return 0;
            }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        sendToSettingsWindow = nullptr;
        sendToList = nullptr;
        sendToAddButton = nullptr;
        sendToEditButton = nullptr;
        sendToRemoveButton = nullptr;
        sendToMoveUpButton = nullptr;
        sendToMoveDownButton = nullptr;
        sendToCloseButton = nullptr;
        EnableWindow(mainWindow, TRUE);
        SetForegroundWindow(mainWindow);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowSendToApplications(HWND owner)
{
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    constexpr int width = 760;
    constexpr int height = 460;
    sendToSettingsWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        SendToSettingsWindowClassName, T(UiText::SendToWindowTitle),
        WS_OVERLAPPEDWINDOW,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
        width, height, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (sendToSettingsWindow == nullptr)
    {
        return;
    }
    EnableWindow(owner, FALSE);
    ShowWindow(sendToSettingsWindow, SW_SHOW);
    UpdateWindow(sendToSettingsWindow);
    MSG message{};
    while (IsWindow(sendToSettingsWindow) &&
           GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(sendToSettingsWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

void UpdateExtinfFormatControls()
{
    if (extinfFormatWindow == nullptr)
        return;
    const int checkedId = ExtinfArtistTitleRadioId +
        static_cast<int>(extinfFormatPreset);
    CheckRadioButton(extinfFormatWindow, ExtinfArtistTitleRadioId,
                     ExtinfCustomRadioId, checkedId);
    SetWindowTextW(extinfCustomText, customExtinfFormat.c_str());
    EnableWindow(extinfEditButton,
                 extinfFormatPreset == ExtinfFormatPreset::Custom);

    const Track* previewTrack = nullptr;
    if (const Playlist* playlist = GetSelectedPlaylist();
        playlist != nullptr && !playlist->tracks.empty())
    {
        const std::vector<int> selectedIndices =
            GetSelectedTrackIndices(trackListView);
        if (!selectedIndices.empty() && selectedIndices.front() >= 0 &&
            selectedIndices.front() < static_cast<int>(playlist->tracks.size()))
        {
            previewTrack = &playlist->tracks[
                static_cast<std::size_t>(selectedIndices.front())];
        }
        else
        {
            previewTrack = &playlist->tracks.front();
        }
    }
    const std::wstring preview = previewTrack == nullptr
        ? L""
        : BuildExtinfText(*previewTrack, extinfFormatPreset,
                          customExtinfFormat);
    SetWindowTextW(extinfPreviewText, preview.c_str());
}

void ShowCustomExtinfEditor(HWND owner)
{
    if (customExtinfEditorWindow != nullptr)
    {
        SetForegroundWindow(customExtinfEditorWindow);
        return;
    }
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    constexpr int width = 620;
    constexpr int height = 205;
    customExtinfEditorWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME, CustomExtinfEditorWindowClassName,
        T(UiText::CustomExtinfWindowTitle),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
        width, height, owner, nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE)),
        nullptr);
    if (customExtinfEditorWindow == nullptr)
        return;
    EnableWindow(owner, FALSE);
    ShowWindow(customExtinfEditorWindow, SW_SHOW);
    UpdateWindow(customExtinfEditorWindow);
    MSG message{};
    while (IsWindow(customExtinfEditorWindow) &&
           GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(customExtinfEditorWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

LRESULT CALLBACK CustomExtinfEditorWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance =
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance;
        const std::wstring formatLabel =
            std::wstring(T(UiText::FormatColumn)) + L":";
        CreateWindowExW(0, WC_STATICW, formatLabel.c_str(),
                        WS_CHILD | WS_VISIBLE, 14, 16, 70, 22,
                        window, nullptr, instance, nullptr);
        customExtinfEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, customExtinfFormat.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            84, 13, 505, 25, window,
            reinterpret_cast<HMENU>(CustomExtinfEditId), instance, nullptr);
        CreateWindowExW(0, WC_STATICW,
                        T(UiText::FieldsHelpOne),
                        WS_CHILD | WS_VISIBLE, 14, 51, 575, 22,
                        window, nullptr, instance, nullptr);
        CreateWindowExW(0, WC_STATICW,
                        T(UiText::FieldsHelpTwo),
                        WS_CHILD | WS_VISIBLE, 55, 74, 534, 22,
                        window, nullptr, instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Ok),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        427, 118, 76, 28, window,
                        reinterpret_cast<HMENU>(CustomExtinfOkId), instance,
                        nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Cancel),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        513, 118, 76, 28, window,
                        reinterpret_cast<HMENU>(CustomExtinfCancelId), instance,
                        nullptr);
        if (customExtinfEdit == nullptr)
            return -1;
        SendMessageW(customExtinfEdit, EM_SETLIMITTEXT, 2048, 0);
        SetFocus(customExtinfEdit);
        SendMessageW(customExtinfEdit, EM_SETSEL, 0, -1);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case CustomExtinfOkId:
        {
            const std::wstring candidate = GetControlText(customExtinfEdit);
            std::wstring error;
            if (!ValidateExtinfFormat(candidate, &error))
            {
                MessageBoxW(window, T(UiText::InvalidExtinfFormat),
                            T(UiText::CustomExtinfWindowTitle),
                            MB_OK | MB_ICONINFORMATION);
                SetFocus(customExtinfEdit);
                return 0;
            }
            if (customExtinfFormat != candidate)
            {
                customExtinfFormat = candidate;
                MarkAppStateDirty();
            }
            UpdateExtinfFormatControls();
            DestroyWindow(window);
            return 0;
        }
        case CustomExtinfCancelId:
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        customExtinfEditorWindow = nullptr;
        customExtinfEdit = nullptr;
        if (extinfFormatWindow != nullptr)
        {
            EnableWindow(extinfFormatWindow, TRUE);
            SetForegroundWindow(extinfFormatWindow);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK ExtinfFormatWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance =
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance;
        constexpr int PresetControlWidth = 500;
        CreateWindowExW(0, WC_STATICW, T(UiText::Preset),
                        WS_CHILD | WS_VISIBLE,
                        16, 16, 90, 22, window, nullptr, instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::ArtistTitlePreset),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON |
                            WS_GROUP,
                        30, 43, PresetControlWidth, 24, window,
                        reinterpret_cast<HMENU>(ExtinfArtistTitleRadioId),
                        instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::TitlePreset),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                        30, 70, PresetControlWidth, 24, window,
                        reinterpret_cast<HMENU>(ExtinfTitleRadioId), instance,
                        nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::ArtistTitleAlbumPreset),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                        30, 97, PresetControlWidth, 24, window,
                        reinterpret_cast<HMENU>(ExtinfArtistTitleAlbumRadioId),
                        instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Custom),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                        30, 124, PresetControlWidth, 24, window,
                        reinterpret_cast<HMENU>(ExtinfCustomRadioId), instance,
                        nullptr);
        const std::wstring customLabel =
            std::wstring(T(UiText::Custom)) + L":";
        CreateWindowExW(0, WC_STATICW, customLabel.c_str(),
                        WS_CHILD | WS_VISIBLE,
                        16, 164, 80, 22, window, nullptr, instance, nullptr);
        extinfCustomText = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, L"",
            WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
            16, 188, 430, 25, window,
            reinterpret_cast<HMENU>(ExtinfCustomTextId), instance, nullptr);
        extinfEditButton = CreateWindowExW(
            0, WC_BUTTONW,
            (std::wstring(T(UiText::Edit)) + L"...").c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            456, 187, 80, 27, window,
            reinterpret_cast<HMENU>(ExtinfEditButtonId), instance, nullptr);
        CreateWindowExW(0, WC_STATICW, T(UiText::Preview),
                        WS_CHILD | WS_VISIBLE,
                        16, 235, 80, 22, window, nullptr, instance, nullptr);
        extinfPreviewText = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_STATICW, L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            16, 259, 520, 32, window,
            reinterpret_cast<HMENU>(ExtinfPreviewTextId), instance, nullptr);
        CreateWindowExW(0, WC_BUTTONW, T(UiText::Close),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        456, 310, 80, 28, window,
                        reinterpret_cast<HMENU>(ExtinfCloseButtonId), instance,
                        nullptr);
        if (extinfCustomText == nullptr || extinfEditButton == nullptr ||
            extinfPreviewText == nullptr)
            return -1;
        UpdateExtinfFormatControls();
        return 0;
    }
    case WM_COMMAND:
    {
        const int command = LOWORD(wParam);
        if (command >= ExtinfArtistTitleRadioId &&
            command <= ExtinfCustomRadioId && HIWORD(wParam) == BN_CLICKED)
        {
            const ExtinfFormatPreset selected =
                static_cast<ExtinfFormatPreset>(
                    command - ExtinfArtistTitleRadioId);
            if (extinfFormatPreset != selected)
            {
                extinfFormatPreset = selected;
                MarkAppStateDirty();
            }
            UpdateExtinfFormatControls();
            return 0;
        }
        if (command == ExtinfEditButtonId)
        {
            ShowCustomExtinfEditor(window);
            return 0;
        }
        if (command == ExtinfCloseButtonId)
        {
            DestroyWindow(window);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        extinfFormatWindow = nullptr;
        extinfCustomText = nullptr;
        extinfEditButton = nullptr;
        extinfPreviewText = nullptr;
        if (mainWindow != nullptr)
        {
            EnableWindow(mainWindow, TRUE);
            SetForegroundWindow(mainWindow);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowExtinfFormatSettings(HWND owner)
{
    if (extinfFormatWindow != nullptr)
    {
        SetForegroundWindow(extinfFormatWindow);
        return;
    }
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    constexpr int width = 570;
    constexpr int height = 390;
    extinfFormatWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME, ExtinfFormatWindowClassName,
        T(UiText::ExtinfWindowTitle),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
        width, height, owner, nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE)),
        nullptr);
    if (extinfFormatWindow == nullptr)
        return;
    EnableWindow(owner, FALSE);
    ShowWindow(extinfFormatWindow, SW_SHOW);
    UpdateWindow(extinfFormatWindow);
    MSG message{};
    while (IsWindow(extinfFormatWindow) &&
           GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(extinfFormatWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

void UpdateTrackColumnsButtons()
{
    const int selected = trackColumnsList == nullptr ? -1 :
        ListView_GetNextItem(trackColumnsList, -1, LVNI_SELECTED);
    EnableWindow(trackColumnsUpButton, selected > 0);
    EnableWindow(trackColumnsDownButton,
                 selected >= 0 &&
                 selected + 1 < static_cast<int>(trackColumnConfigs.size()));
}

void RefreshTrackColumnsSettingsList(int selectedIndex = -1)
{
    if (trackColumnsList == nullptr)
    {
        return;
    }
    isRefreshingTrackColumns = true;
    ListView_DeleteAllItems(trackColumnsList);
    for (std::size_t index = 0; index < trackColumnConfigs.size(); ++index)
    {
        const TrackColumnConfig& config = trackColumnConfigs[index];
        InsertListItem(trackColumnsList, static_cast<int>(index),
                       GetTrackColumnName(config.id));
        ListView_SetCheckState(trackColumnsList, static_cast<int>(index),
                               config.visible ? TRUE : FALSE);
    }
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(trackColumnConfigs.size()))
    {
        ListView_SetItemState(trackColumnsList, selectedIndex,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(trackColumnsList, selectedIndex, FALSE);
    }
    isRefreshingTrackColumns = false;
    UpdateTrackColumnsButtons();
}

void MoveTrackColumnSetting(int direction)
{
    const int selected = ListView_GetNextItem(
        trackColumnsList, -1, LVNI_SELECTED);
    const int destination = selected + direction;
    if (selected < 0 || destination < 0 ||
        destination >= static_cast<int>(trackColumnConfigs.size()))
    {
        return;
    }
    SaveCurrentTrackColumnWidths();
    std::swap(trackColumnConfigs[static_cast<std::size_t>(selected)],
              trackColumnConfigs[static_cast<std::size_t>(destination)]);
    RefreshTrackColumnsSettingsList(destination);
    RebuildTrackListColumns();
    MarkAppStateDirty();
}

void LayoutTrackColumnsWindow(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    constexpr int margin = 12;
    constexpr int buttonWidth = 74;
    constexpr int buttonHeight = 28;
    constexpr int gap = 8;
    const int clientRight = static_cast<int>(client.right);
    const int clientBottom = static_cast<int>(client.bottom);
    const int rightX = std::max(margin, clientRight - margin - buttonWidth);
    const int listWidth = std::max(120, rightX - margin - gap);
    const int listHeight = std::max(80, clientBottom - margin * 2);
    MoveWindow(trackColumnsList, margin, margin, listWidth, listHeight, TRUE);
    if (trackColumnsList != nullptr)
    {
        ListView_SetColumnWidth(trackColumnsList, 0,
                                std::max(80, listWidth - 4));
    }
    MoveWindow(trackColumnsUpButton, rightX, margin, buttonWidth,
               buttonHeight, TRUE);
    MoveWindow(trackColumnsDownButton, rightX, margin + buttonHeight + gap,
               buttonWidth, buttonHeight, TRUE);
    MoveWindow(trackColumnsCloseButton, rightX,
               std::max(margin, clientBottom - margin - buttonHeight),
               buttonWidth, buttonHeight, TRUE);
}

LRESULT CALLBACK TrackColumnsWindowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const HINSTANCE instance =
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance;
        trackColumnsList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
                LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(TrackColumnsListId), instance, nullptr);
        trackColumnsUpButton = CreateWindowExW(
            0, WC_BUTTONW, L"\u2191",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(TrackColumnsUpButtonId), instance, nullptr);
        trackColumnsDownButton = CreateWindowExW(
            0, WC_BUTTONW, L"\u2193",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(TrackColumnsDownButtonId), instance, nullptr);
        trackColumnsCloseButton = CreateWindowExW(
            0, WC_BUTTONW, T(UiText::Close),
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(TrackColumnsCloseButtonId), instance, nullptr);
        if (trackColumnsList == nullptr || trackColumnsUpButton == nullptr ||
            trackColumnsDownButton == nullptr ||
            trackColumnsCloseButton == nullptr)
        {
            return -1;
        }
        ListView_SetExtendedListViewStyle(
            trackColumnsList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT |
                                  LVS_EX_DOUBLEBUFFER);
        InsertColumn(trackColumnsList, 0, T(UiText::ColumnHeader), 250);
        RefreshTrackColumnsSettingsList();
        LayoutTrackColumnsWindow(window);
        return 0;
    }
    case WM_SIZE:
        LayoutTrackColumnsWindow(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case TrackColumnsUpButtonId:
            MoveTrackColumnSetting(-1);
            return 0;
        case TrackColumnsDownButtonId:
            MoveTrackColumnSetting(1);
            return 0;
        case TrackColumnsCloseButtonId:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_NOTIFY:
    {
        const NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->hwndFrom == trackColumnsList &&
            header->code == LVN_ITEMCHANGED && !isRefreshingTrackColumns)
        {
            const NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((change->uChanged & LVIF_STATE) != 0 &&
                ((change->uOldState ^ change->uNewState) &
                 LVIS_STATEIMAGEMASK) != 0 &&
                change->iItem >= 0 &&
                change->iItem < static_cast<int>(trackColumnConfigs.size()))
            {
                TrackColumnConfig& config = trackColumnConfigs[
                    static_cast<std::size_t>(change->iItem)];
                bool checked = ListView_GetCheckState(
                    trackColumnsList, change->iItem) != FALSE;
                if (config.id == TrackColumnId::Title && !checked)
                {
                    isRefreshingTrackColumns = true;
                    ListView_SetCheckState(trackColumnsList, change->iItem,
                                           TRUE);
                    isRefreshingTrackColumns = false;
                }
                else if (config.visible != checked)
                {
                    SaveCurrentTrackColumnWidths();
                    config.visible = checked;
                    RebuildTrackListColumns();
                    MarkAppStateDirty();
                }
            }
            UpdateTrackColumnsButtons();
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        trackColumnsWindow = nullptr;
        trackColumnsList = nullptr;
        trackColumnsUpButton = nullptr;
        trackColumnsDownButton = nullptr;
        trackColumnsCloseButton = nullptr;
        if (mainWindow != nullptr)
        {
            EnableWindow(mainWindow, TRUE);
            SetForegroundWindow(mainWindow);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowTrackColumns(HWND owner)
{
    if (trackColumnsWindow != nullptr)
    {
        SetForegroundWindow(trackColumnsWindow);
        return;
    }
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    constexpr int width = 430;
    constexpr int height = 510;
    trackColumnsWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME, TrackColumnsWindowClassName,
        T(UiText::TrackColumnsWindowTitle),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
        width, height, owner, nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE)),
        nullptr);
    if (trackColumnsWindow == nullptr)
    {
        return;
    }
    EnableWindow(owner, FALSE);
    ShowWindow(trackColumnsWindow, SW_SHOW);
    UpdateWindow(trackColumnsWindow);
    MSG message{};
    while (IsWindow(trackColumnsWindow) &&
           GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(trackColumnsWindow, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        if (!CreateMainMenuBar(window))
        {
            return -1;
        }
        playlistListView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
                LVS_SHOWSELALWAYS | LVS_EDITLABELS,
            0, 0, 0, 0, window, nullptr,
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance, nullptr);
        trackListView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window, nullptr,
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance, nullptr);
        statusText = CreateWindowExW(
            WS_EX_STATICEDGE, WC_STATICW, L"",
            WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
            0, 0, 0, 0, window, nullptr,
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance, nullptr);

        if (playlistListView == nullptr || trackListView == nullptr ||
            statusText == nullptr)
        {
            return -1;
        }
        if (!InitializeListTooltip(window, playlistListView,
                                   playlistTooltipState,
                                   PlaylistTooltipSubclassId) ||
            !InitializeListTooltip(window, trackListView,
                                   trackTooltipState,
                                   TrackTooltipSubclassId))
        {
            return -1;
        }
        playlistTooltip = playlistTooltipState.tooltip;
        trackTooltip = trackTooltipState.tooltip;

        InitializeStatusFont();

        ListView_SetExtendedListViewStyle(
            playlistListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetExtendedListViewStyle(
            trackListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        InsertColumn(playlistListView, 0, T(UiText::PlaylistHeader), splitterX);
        RefreshPlaylistList(playlistListView);
        RebuildTrackListColumns();
        DragAcceptFiles(window, TRUE);
        SetTimer(window, AppStateTimerId, AppStateTimerIntervalMs, nullptr);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) >= SendToApplicationCommandBase &&
            LOWORD(wParam) < SendToApplicationCommandBase +
                MaximumSendToApplications)
        {
            SendSelectedTracksToApplication(
                window, static_cast<int>(LOWORD(wParam) -
                                         SendToApplicationCommandBase));
            return 0;
        }
        switch (LOWORD(wParam))
        {
        case CommandOpenAudioFiles:
            OpenAudioFiles(window);
            return 0;
        case CommandImportPlaylist:
            ImportPlaylistFromDialog(window);
            return 0;
        case CommandExit:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        case CommandOrganizePlaylists:
            ShowPlaylistOrganizer(window);
            return 0;
        case CommandSendToApplications:
            ShowSendToApplications(window);
            return 0;
        case CommandTrackColumns:
            ShowTrackColumns(window);
            return 0;
        case CommandExtinfFormat:
            ShowExtinfFormatSettings(window);
            return 0;
        case CommandOnlineManual:
            OpenOnlineManual(window);
            return 0;
        case CommandAbout:
        {
            const std::wstring about = std::wstring(T(UiText::AboutBodyPrefix)) +
                APP_VERSION_WSTRING + T(UiText::AboutBodySuffix);
            MessageBoxW(
                window, about.c_str(), T(UiText::AboutTitle),
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case CommandLanguageEnglish:
        case CommandLanguageJapanese:
        {
            const AppLanguage selected = LOWORD(wParam) ==
                CommandLanguageJapanese ? AppLanguage::Japanese
                                        : AppLanguage::English;
            if (languageSetting == selected)
                return 0;
            languageSetting = selected;
            CheckMenuRadioItem(
                languageMenu, CommandLanguageEnglish,
                CommandLanguageJapanese, LOWORD(wParam), MF_BYCOMMAND);
            MarkAppStateDirty();
            const bool saved = SaveCurrentAppState();
            MessageBoxW(window,
                        T(saved ? UiText::LanguageSaved
                                : UiText::LanguageSaveFailed),
                        WindowTitle,
                        MB_OK | (saved ? MB_ICONINFORMATION : MB_ICONERROR));
            return 0;
        }
        case CommandNewPlaylist:
            CreateNewPlaylist();
            return 0;
        case CommandRenamePlaylist:
            RenameSelectedPlaylist();
            return 0;
        case CommandDeletePlaylist:
            DeleteSelectedPlaylist(window);
            return 0;
        case CommandExportM3U8:
            ExportSelectedPlaylist(window);
            return 0;
        case CommandGetTrackMetadata:
            GetMetadataForSelectedTracks();
            return 0;
        case CommandDeleteTracks:
            DeleteSelectedTracks();
            return 0;
        case CommandOpenTracksInExplorer:
            OpenSelectedTracksInExplorer();
            return 0;
        case CommandShowTrackProperties:
            ShowSelectedTrackProperties();
            return 0;
        }
        break;

    case WM_INITMENUPOPUP:
        if (reinterpret_cast<HMENU>(wParam) != trackSendToMenu)
        {
            UpdateMainMenuState();
        }
        return 0;

    case WM_NOTIFY:
    {
        NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
        const HWND trackHeader = trackListView == nullptr
                                     ? nullptr
                                     : ListView_GetHeader(trackListView);
        if (header->hwndFrom == trackHeader &&
            (header->code == HDN_ENDTRACKW ||
             header->code == HDN_ENDTRACKA))
        {
            SaveCurrentTrackColumnWidths();
            MarkAppStateDirty();
            return 0;
        }
        if (header->hwndFrom == trackListView &&
            header->code == LVN_ITEMCHANGED && !isRefreshingTrackList)
        {
            const NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((change->uChanged & LVIF_STATE) != 0 &&
                ((change->uOldState ^ change->uNewState) & LVIS_SELECTED) != 0)
            {
                RefreshStatusBar();
            }
            return 0;
        }
        if (header->hwndFrom == trackListView &&
            header->code == NM_DBLCLK)
        {
            if (sendToApplications.empty())
                return 0;
            const NMITEMACTIVATE* activation =
                reinterpret_cast<NMITEMACTIVATE*>(lParam);
            LVHITTESTINFO hitTest{};
            hitTest.pt = activation->ptAction;
            const int row = ListView_SubItemHitTest(trackListView, &hitTest);
            if (row >= 0)
                SendTracksToApplication(window, {row}, 0);
            return 0;
        }
        if (header->code == LVN_BEGINDRAG)
        {
            NMLISTVIEW* drag = reinterpret_cast<NMLISTVIEW*>(lParam);
            if (header->hwndFrom == playlistListView)
            {
                return 0;
            }
            if (header->hwndFrom == trackListView)
            {
                StartSelectedTrackDrag(drag->iItem);
                return 0;
            }
        }
        if (header->hwndFrom == trackListView && header->code == LVN_KEYDOWN)
        {
            NMLVKEYDOWN* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
            if (key->wVKey == VK_DELETE)
            {
                DeleteSelectedTracks();
            }
            return 0;
        }
        if (header->hwndFrom != playlistListView)
        {
            break;
        }

        if (header->code == LVN_ITEMCHANGED && !isRefreshingPlaylistList)
        {
            NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(lParam);
            const int playlistIndex =
                GetPlaylistIndexFromVisibleRow(change->iItem);
            const bool becameSelected =
                (change->uChanged & LVIF_STATE) != 0 &&
                (change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0 &&
                playlistIndex >= 0;
            if (becameSelected)
            {
                if (selectedPlaylistIndex != playlistIndex)
                {
                    selectedPlaylistIndex = playlistIndex;
                    MarkAppStateDirty();
                }
                RefreshSelectedTrackList();
            }
            return 0;
        }

        if (header->code == LVN_ENDLABELEDITW)
        {
            NMLVDISPINFOW* edit = reinterpret_cast<NMLVDISPINFOW*>(lParam);
            const int playlistIndex =
                GetPlaylistIndexFromVisibleRow(edit->item.iItem);
            if (playlistIndex >= 0 && edit->item.pszText != nullptr &&
                HasVisibleName(edit->item.pszText))
            {
                selectedPlaylistIndex = playlistIndex;
                playlists[static_cast<std::size_t>(playlistIndex)].name =
                    edit->item.pszText;
                playlists[static_cast<std::size_t>(playlistIndex)].isModified =
                    true;
                MarkAppStateDirty();
                PostMessageW(window, MessageRefreshPlaylistList, 0, 0);
            }
            return FALSE;
        }

        if (header->code == LVN_BEGINLABELEDITW)
        {
            const NMLVDISPINFOW* edit =
                reinterpret_cast<NMLVDISPINFOW*>(lParam);
            const int playlistIndex =
                GetPlaylistIndexFromVisibleRow(edit->item.iItem);
            if (playlistIndex < 0)
            {
                return TRUE;
            }
            PostMessageW(window, MessagePreparePlaylistLabelEdit,
                         static_cast<WPARAM>(playlistIndex), 0);
            return FALSE;
        }

        if (header->code == NM_CLICK)
        {
            const NMITEMACTIVATE* click =
                reinterpret_cast<NMITEMACTIVATE*>(lParam);
            const int groupIndex = GetGroupIndexFromVisibleRow(click->iItem);
            if (groupIndex >= 0 &&
                groupIndex < static_cast<int>(playlistGroups.size()))
            {
                PostMessageW(window, MessageTogglePlaylistGroup,
                             static_cast<WPARAM>(groupIndex), 0);
            }
            return 0;
        }

        if (header->code == LVN_KEYDOWN)
        {
            NMLVKEYDOWN* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
            const int focusedRow = ListView_GetNextItem(
                playlistListView, -1, LVNI_FOCUSED);
            const bool playlistRowFocused =
                GetPlaylistIndexFromVisibleRow(focusedRow) >= 0;
            if (key->wVKey == VK_F2 && playlistRowFocused)
            {
                RenameSelectedPlaylist();
            }
            else if (key->wVKey == VK_DELETE && playlistRowFocused)
            {
                DeleteSelectedPlaylist(window);
            }
            return 0;
        }
        break;
    }

    case MessageRefreshPlaylistList:
        RefreshPlaylistList(playlistListView);
        return 0;

    case MessagePreparePlaylistLabelEdit:
    {
        const int playlistIndex = static_cast<int>(wParam);
        const HWND editControl = ListView_GetEditControl(playlistListView);
        if (editControl != nullptr && playlistIndex >= 0 &&
            playlistIndex < static_cast<int>(playlists.size()))
        {
            SetWindowTextW(
                editControl,
                playlists[static_cast<std::size_t>(playlistIndex)].name.c_str());
            SendMessageW(editControl, EM_SETSEL, 0, -1);
        }
        return 0;
    }

    case MessageTogglePlaylistGroup:
    {
        const int groupIndex = static_cast<int>(wParam);
        if (groupIndex >= 0 &&
            groupIndex < static_cast<int>(playlistGroups.size()))
        {
            PlaylistGroup& group =
                playlistGroups[static_cast<std::size_t>(groupIndex)];
            group.expanded = !group.expanded;
            MarkAppStateDirty();
            RefreshPlaylistList(playlistListView);
        }
        return 0;
    }

    case WM_CONTEXTMENU:
        ResetListTooltip(playlistTooltipState);
        ResetListTooltip(trackTooltipState);
        if (reinterpret_cast<HWND>(wParam) == playlistListView)
        {
            ShowPlaylistContextMenu(window, lParam);
            return 0;
        }
        if (reinterpret_cast<HWND>(wParam) == trackListView)
        {
            ShowTrackContextMenu(window, lParam);
            return 0;
        }
        break;

    case WM_SIZE:
        LayoutChildren(window);
        if (wParam != SIZE_MINIMIZED)
        {
            RECT windowRect{};
            GetWindowRect(window, &windowRect);
            const int width = windowRect.right - windowRect.left;
            const int height = windowRect.bottom - windowRect.top;
            if (width >= MinimumWindowWidth && height >= MinimumWindowHeight &&
                (width != savedWindowWidth || height != savedWindowHeight))
            {
                savedWindowWidth = width;
                savedWindowHeight = height;
                MarkAppStateDirty();
            }
        }
        return 0;

    case WM_MOVE:
        if (wParam == 0 && !IsIconic(window))
        {
            RECT windowRect{};
            if (GetWindowRect(window, &windowRect) &&
                (!hasSavedWindowPosition ||
                 windowRect.left != savedWindowX ||
                 windowRect.top != savedWindowY))
            {
                savedWindowX = windowRect.left;
                savedWindowY = windowRect.top;
                hasSavedWindowPosition = true;
                MarkAppStateDirty();
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
    {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsPointOnSplitter(window, point))
        {
            isDraggingSplitter = true;
            splitterXAtDragStart = splitterX;
            splitterDragOffset = point.x - splitterX;
            SetCapture(window);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (isDraggingSplitter)
        {
            splitterX = GET_X_LPARAM(lParam) - splitterDragOffset;
            LayoutChildren(window);
        }
        return 0;

    case WM_LBUTTONUP:
        if (isDraggingSplitter)
        {
            isDraggingSplitter = false;
            ReleaseCapture();
            if (splitterX != splitterXAtDragStart)
            {
                MarkAppStateDirty();
            }
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (isDraggingSplitter && splitterX != splitterXAtDragStart)
        {
            MarkAppStateDirty();
        }
        isDraggingSplitter = false;
        return 0;

    case WM_CANCELMODE:
        if (isDraggingSplitter && splitterX != splitterXAtDragStart)
        {
            MarkAppStateDirty();
        }
        isDraggingSplitter = false;
        if (GetCapture() == window)
        {
            ReleaseCapture();
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(window, &point);
            if (IsPointOnSplitter(window, point))
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_DROPFILES:
        HandleDroppedFiles(window, reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_TIMER:
        if (wParam == AppStateTimerId && appStateDirty)
        {
            SaveCurrentAppState();
        }
        return 0;

    case WM_CLOSE:
        if (appStateDirty && !SaveCurrentAppState())
        {
            MessageBoxW(window,
                        T(UiText::StateSaveFailed),
                        WindowTitle, MB_OK | MB_ICONWARNING);
        }
        DestroyWindow(window);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC deviceContext = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        RECT splitterRect{splitterX, 0, splitterX + SplitterWidth,
                          std::max(0, static_cast<int>(client.bottom) -
                                          statusHeight)};
        FillRect(deviceContext, &splitterRect,
                 GetSysColorBrush(COLOR_3DFACE));
        EndPaint(window, &paint);
        return 0;
    }

    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {
            MinimumWindowWidth, MinimumWindowHeight
        };
        return 0;

    case WM_DESTROY:
        KillTimer(window, AppStateTimerId);
        if (playlistTooltip != nullptr)
        {
            DestroyWindow(playlistTooltip);
            playlistTooltip = nullptr;
        }
        if (trackTooltip != nullptr)
        {
            DestroyWindow(trackTooltip);
            trackTooltip = nullptr;
        }
        if (statusFont != nullptr)
        {
            DeleteObject(statusFont);
            statusFont = nullptr;
        }
        mainWindow = nullptr;
        fileMenu = nullptr;
        playlistMenu = nullptr;
        trackMenu = nullptr;
        trackSendToMenu = nullptr;
        settingsMenu = nullptr;
        helpMenu = nullptr;
        languageMenu = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    OleApartment oleApartment;
    oleDragDropAvailable = oleApartment.IsAvailable();

    AppState loadedState{};
    const AppStateLoadResult loadResult = LoadAppState(loadedState);
    if (loadResult == AppStateLoadResult::Loaded)
    {
        ApplyLoadedAppState(std::move(loadedState));
    }
    else if (loadResult == AppStateLoadResult::Failed)
    {
        ResetToDefaultAppState();
        MessageBoxW(nullptr,
                    T(UiText::StateLoadFailed),
                    WindowTitle, MB_OK | MB_ICONWARNING);
    }
    KeepSavedWindowPositionOnScreen();

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&commonControls))
    {
        MessageBoxW(nullptr, T(UiText::CommonControlsFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_SHARED));
    if (windowClass.hIcon == nullptr)
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (windowClass.hIconSm == nullptr)
        windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.lpszClassName = WindowClassName;

    if (!RegisterClassExW(&windowClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW organizerClass = windowClass;
    organizerClass.lpfnWndProc = PlaylistOrganizerWindowProcedure;
    organizerClass.lpszClassName = OrganizerWindowClassName;
    organizerClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&organizerClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW sendToSettingsClass = windowClass;
    sendToSettingsClass.lpfnWndProc = SendToSettingsWindowProcedure;
    sendToSettingsClass.lpszClassName = SendToSettingsWindowClassName;
    sendToSettingsClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&sendToSettingsClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW sendToEditorClass = windowClass;
    sendToEditorClass.lpfnWndProc = SendToEditorWindowProcedure;
    sendToEditorClass.lpszClassName = SendToEditorWindowClassName;
    sendToEditorClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&sendToEditorClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW trackColumnsClass = windowClass;
    trackColumnsClass.lpfnWndProc = TrackColumnsWindowProcedure;
    trackColumnsClass.lpszClassName = TrackColumnsWindowClassName;
    trackColumnsClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&trackColumnsClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW extinfFormatClass = windowClass;
    extinfFormatClass.lpfnWndProc = ExtinfFormatWindowProcedure;
    extinfFormatClass.lpszClassName = ExtinfFormatWindowClassName;
    extinfFormatClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&extinfFormatClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW customExtinfEditorClass = windowClass;
    customExtinfEditorClass.lpfnWndProc =
        CustomExtinfEditorWindowProcedure;
    customExtinfEditorClass.lpszClassName =
        CustomExtinfEditorWindowClassName;
    customExtinfEditorClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&customExtinfEditorClass))
    {
        MessageBoxW(nullptr, T(UiText::WindowClassFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND window = CreateWindowExW(
        0, WindowClassName, WindowTitle, WS_OVERLAPPEDWINDOW,
        hasSavedWindowPosition ? savedWindowX : CW_USEDEFAULT,
        hasSavedWindowPosition ? savedWindowY : CW_USEDEFAULT,
        savedWindowWidth, savedWindowHeight,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        MessageBoxW(nullptr, T(UiText::MainWindowFailed),
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    mainWindow = window;

    ShowWindow(window, showCommand);
    UpdateWindow(window);
    appStateTrackingEnabled = true;

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (HandleAltArrowKey(message))
        {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
