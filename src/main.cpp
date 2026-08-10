#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "app_state.h"
#include "drag_drop.h"
#include "playlist.h"

namespace
{
constexpr wchar_t WindowClassName[] = L"MusicPlaylistManagerWindow";
constexpr wchar_t WindowTitle[] = L"Music Playlist Manager";
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
constexpr UINT MessageRefreshPlaylistList = WM_APP + 1;
constexpr UINT_PTR AppStateTimerId = 1;
constexpr UINT AppStateTimerIntervalMs = 60'000;

HWND mainWindow = nullptr;
HWND playlistListView = nullptr;
HWND trackListView = nullptr;
int splitterX = 240;
int savedWindowX = 0;
int savedWindowY = 0;
bool hasSavedWindowPosition = false;
int savedWindowWidth = 900;
int savedWindowHeight = 600;
std::array<int, TrackColumnCount> trackColumnWidths =
    DefaultTrackColumnWidths;
bool isDraggingSplitter = false;
int splitterDragOffset = 0;
int splitterXAtDragStart = 240;
bool oleDragDropAvailable = false;

std::vector<Playlist> playlists{{L"New Playlist", L"", {}, false}};
int selectedPlaylistIndex = 0;
bool isRefreshingPlaylistList = false;
bool appStateDirty = false;
bool appStateTrackingEnabled = false;

void MarkAppStateDirty()
{
    if (appStateTrackingEnabled)
    {
        appStateDirty = true;
    }
}

AppState CaptureCurrentAppState()
{
    AppState state{};
    state.playlists = playlists;
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
    state.trackColumnWidths = trackColumnWidths;
    if (trackListView != nullptr)
    {
        for (std::size_t index = 0; index < TrackColumnCount; ++index)
        {
            const int width = ListView_GetColumnWidth(
                trackListView, static_cast<int>(index));
            if (width > 0)
            {
                state.trackColumnWidths[index] = width;
            }
        }
    }
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
    playlists = std::move(state.playlists);
    selectedPlaylistIndex = state.selectedPlaylistIndex;
    splitterX = state.splitterX;
    savedWindowX = state.windowX;
    savedWindowY = state.windowY;
    hasSavedWindowPosition = state.hasWindowPosition;
    savedWindowWidth = state.windowWidth;
    savedWindowHeight = state.windowHeight;
    trackColumnWidths = state.trackColumnWidths;
}

void ResetToDefaultAppState()
{
    playlists = {{L"New Playlist", L"", {}, false}};
    selectedPlaylistIndex = 0;
    splitterX = 240;
    savedWindowX = 0;
    savedWindowY = 0;
    hasSavedWindowPosition = false;
    savedWindowWidth = 900;
    savedWindowHeight = 600;
    trackColumnWidths = DefaultTrackColumnWidths;
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

void RefreshPlaylistList(HWND listView,
                         const std::vector<Playlist>& playlistData)
{
    isRefreshingPlaylistList = true;
    ListView_DeleteAllItems(listView);
    for (std::size_t index = 0; index < playlistData.size(); ++index)
    {
        InsertListItem(listView, static_cast<int>(index),
                       playlistData[index].name);
    }

    if (selectedPlaylistIndex >= 0 &&
        selectedPlaylistIndex < static_cast<int>(playlistData.size()))
    {
        ListView_SetItemState(listView, selectedPlaylistIndex,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(listView, selectedPlaylistIndex, FALSE);
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

void RefreshTrackList(HWND listView, const Playlist& playlist)
{
    ListView_DeleteAllItems(listView);
    for (std::size_t index = 0; index < playlist.tracks.size(); ++index)
    {
        const int row = static_cast<int>(index);
        const Track& track = playlist.tracks[index];
        const std::wstring displayTitle = track.title.empty()
            ? track.extinfText
            : track.title;
        const std::wstring displayDuration = track.duration.empty()
            ? FormatDuration(track.extinfDuration)
            : track.duration;
        InsertListItem(listView, row, displayTitle);
        SetListItemText(listView, row, 1, track.artist);
        SetListItemText(listView, row, 2, track.album);
        SetListItemText(listView, row, 3, displayDuration);
        SetListItemText(listView, row, 4, track.path);
    }
}

void RefreshSelectedTrackList()
{
    if (const Playlist* playlist = GetSelectedPlaylist())
    {
        RefreshTrackList(trackListView, *playlist);
    }
    else
    {
        ListView_DeleteAllItems(trackListView);
    }
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

std::vector<std::wstring> GetSelectedExistingTrackPaths()
{
    std::vector<std::wstring> paths;
    const Playlist* playlist = GetSelectedPlaylist();
    if (playlist == nullptr)
    {
        return paths;
    }

    for (const int index : GetSelectedTrackIndices(trackListView))
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

    const int destinationIndex = selectedPlaylistIndex + direction;
    if (destinationIndex < 0 ||
        destinationIndex >= static_cast<int>(playlists.size()))
    {
        return false;
    }

    std::swap(playlists[static_cast<std::size_t>(selectedPlaylistIndex)],
              playlists[static_cast<std::size_t>(destinationIndex)]);
    selectedPlaylistIndex = destinationIndex;
    RefreshPlaylistList(playlistListView, playlists);
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

    bool updatedAnyTrack = false;
    for (const int index : selectedIndices)
    {
        if (index >= 0 && index < static_cast<int>(playlist->tracks.size()))
        {
            updatedAnyTrack =
                UpdateTrackMetadata(
                    playlist->tracks[static_cast<std::size_t>(index)]) ||
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
    playlists.push_back(Playlist{MakeNewPlaylistName(), L"", {}, false});
    selectedPlaylistIndex = static_cast<int>(playlists.size()) - 1;
    MarkAppStateDirty();
    RefreshPlaylistList(playlistListView, playlists);
    RefreshSelectedTrackList();
    SetFocus(playlistListView);
    ListView_EditLabel(playlistListView, selectedPlaylistIndex);
}

void RenameSelectedPlaylist()
{
    if (GetSelectedPlaylist() == nullptr)
    {
        return;
    }

    SetFocus(playlistListView);
    ListView_EditLabel(playlistListView, selectedPlaylistIndex);
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
            L"This playlist contains " +
            std::to_wstring(selectedPlaylist->tracks.size()) +
            L" tracks.\nDelete this playlist?";
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
    RefreshPlaylistList(playlistListView, playlists);
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
        MessageBoxW(window, L"No playlist selected.", WindowTitle,
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    constexpr DWORD MaximumPathLength = 32768;
    std::vector<wchar_t> filePathBuffer(MaximumPathLength, L'\0');
    const std::wstring initialPath = playlist->filePath.empty()
        ? MakeSafeExportFileName(playlist->name)
        : playlist->filePath;
    const std::size_t copyLength =
        std::min(initialPath.size(), filePathBuffer.size() - 1);
    std::copy_n(initialPath.data(), copyLength, filePathBuffer.data());

    constexpr wchar_t FileFilter[] =
        L"m3u8 Playlist (*.m3u8)\0*.m3u8\0"
        L"All Files (*.*)\0*.*\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = FileFilter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = filePathBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(filePathBuffer.size());
    dialog.lpstrDefExt = L"m3u8";
    dialog.lpstrTitle = L"Export Playlist as m3u8";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(window, L"The Save dialog could not be opened.",
                        WindowTitle, MB_OK | MB_ICONERROR);
        }
        return;
    }

    try
    {
        SaveM3U8(*playlist, filePathBuffer.data());
        playlist->filePath = filePathBuffer.data();
        playlist->isModified = false;
        MarkAppStateDirty();
        MessageBoxW(window, L"The playlist was exported successfully.",
                    WindowTitle, MB_OK | MB_ICONINFORMATION);
    }
    catch (const std::exception&)
    {
        MessageBoxW(window, L"Failed to export the m3u8 playlist.",
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
    ListView_SetItemState(playlistListView, index,
                          LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    RefreshSelectedTrackList();
}

void ShowPlaylistContextMenu(HWND window, LPARAM lParam)
{
    POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        RECT itemRect{};
        if (selectedPlaylistIndex >= 0 &&
            ListView_GetItemRect(playlistListView, selectedPlaylistIndex,
                                 &itemRect, LVIR_BOUNDS))
        {
            screenPoint = {itemRect.left, itemRect.bottom};
            ClientToScreen(playlistListView, &screenPoint);
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
        if (hitIndex >= 0)
        {
            SelectPlaylist(hitIndex);
        }
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING, CommandNewPlaylist, L"New Playlist");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const UINT selectionState =
        GetSelectedPlaylist() == nullptr ? MF_GRAYED : MF_ENABLED;
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandRenamePlaylist, L"Rename");
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandDeletePlaylist, L"Delete");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandExportM3U8, L"Export m3u8...");

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
    if (menu == nullptr)
    {
        return;
    }

    const UINT selectionState = GetSelectedTrackIndices(trackListView).empty()
        ? MF_GRAYED
        : MF_ENABLED;
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandGetTrackMetadata, L"Get Metadata");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | selectionState,
                CommandDeleteTracks, L"Delete Track");

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

    MoveWindow(playlistListView, 0, 0, splitterX, height, TRUE);
    MoveWindow(trackListView, rightX, 0, rightWidth, height, TRUE);

    ListView_SetColumnWidth(playlistListView, 0, std::max(0, splitterX - 4));
    InvalidateRect(window, nullptr, FALSE);
}

bool IsPointOnSplitter(HWND window, POINT point)
{
    RECT client{};
    GetClientRect(window, &client);
    return point.x >= splitterX && point.x < splitterX + SplitterWidth &&
           point.y >= 0 && point.y < client.bottom;
}

bool IsPointInTrackPane(HWND window, POINT point)
{
    RECT trackRect{};
    GetWindowRect(trackListView, &trackRect);
    MapWindowPoints(HWND_DESKTOP, window,
                    reinterpret_cast<POINT*>(&trackRect), 2);
    return PtInRect(&trackRect, point) != FALSE;
}

enum class M3U8LoadMode
{
    Cancel,
    NewPlaylist,
    AppendToCurrent
};

M3U8LoadMode ChooseM3U8LoadMode(HWND window, const std::wstring& filePath)
{
    if (GetSelectedPlaylist() == nullptr)
    {
        const std::wstring message =
            L"No playlist is selected.\n\nLoad this m3u8 as a new playlist?\n\n" +
            filePath;
        return MessageBoxW(window, message.c_str(), WindowTitle,
                           MB_OKCANCEL | MB_ICONQUESTION) == IDOK
            ? M3U8LoadMode::NewPlaylist
            : M3U8LoadMode::Cancel;
    }

    const std::wstring message =
        L"Load this m3u8 playlist?\n\n" + filePath +
        L"\n\nYes: Add as a new playlist"
        L"\nNo: Append to the current playlist"
        L"\nCancel: Do nothing";
    switch (MessageBoxW(window, message.c_str(), WindowTitle,
                        MB_YESNOCANCEL | MB_ICONQUESTION))
    {
    case IDYES:
        return M3U8LoadMode::NewPlaylist;
    case IDNO:
        return M3U8LoadMode::AppendToCurrent;
    default:
        return M3U8LoadMode::Cancel;
    }
}

void ImportM3U8(HWND window, const std::wstring& filePath)
{
    const M3U8LoadMode mode = ChooseM3U8LoadMode(window, filePath);
    if (mode == M3U8LoadMode::Cancel)
    {
        return;
    }

    Playlist loadedPlaylist;
    try
    {
        loadedPlaylist = LoadM3U8(filePath);
    }
    catch (const std::exception&)
    {
        MessageBoxW(window,
                    L"Failed to load the m3u8 file.\n"
                    L"The file must be readable UTF-8 text.",
                    WindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    if (mode == M3U8LoadMode::NewPlaylist)
    {
        playlists.push_back(std::move(loadedPlaylist));
        selectedPlaylistIndex = static_cast<int>(playlists.size()) - 1;
        MarkAppStateDirty();
        RefreshPlaylistList(playlistListView, playlists);
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
            if (IsM3U8Path(path))
            {
                ImportM3U8(window, path);
                continue;
            }
            if (IsSupportedAudioPath(path))
            {
                if (Playlist* selectedPlaylist = GetSelectedPlaylist())
                {
                    AddTrack(*selectedPlaylist, CreateTrackFromFile(path));
                    addedAudioTrack = true;
                }
                else if (!showedNoPlaylistMessage)
                {
                    MessageBoxW(window,
                                L"No playlist selected. Create a playlist first.",
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

LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
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

        if (playlistListView == nullptr || trackListView == nullptr)
        {
            return -1;
        }

        ListView_SetExtendedListViewStyle(
            playlistListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetExtendedListViewStyle(
            trackListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        InsertColumn(playlistListView, 0, L"Playlist", splitterX);
        InsertColumn(trackListView, 0, L"Title", trackColumnWidths[0]);
        InsertColumn(trackListView, 1, L"Artist", trackColumnWidths[1]);
        InsertColumn(trackListView, 2, L"Album", trackColumnWidths[2]);
        InsertColumn(trackListView, 3, L"Duration", trackColumnWidths[3]);
        InsertColumn(trackListView, 4, L"Path", trackColumnWidths[4]);
        RefreshPlaylistList(playlistListView, playlists);
        RefreshSelectedTrackList();
        DragAcceptFiles(window, TRUE);
        SetTimer(window, AppStateTimerId, AppStateTimerIntervalMs, nullptr);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
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
        }
        break;

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
            MarkAppStateDirty();
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
            const bool becameSelected =
                (change->uChanged & LVIF_STATE) != 0 &&
                (change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0 &&
                change->iItem >= 0 &&
                change->iItem < static_cast<int>(playlists.size());
            if (becameSelected)
            {
                if (selectedPlaylistIndex != change->iItem)
                {
                    selectedPlaylistIndex = change->iItem;
                    MarkAppStateDirty();
                }
                RefreshSelectedTrackList();
            }
            return 0;
        }

        if (header->code == LVN_ENDLABELEDITW)
        {
            NMLVDISPINFOW* edit = reinterpret_cast<NMLVDISPINFOW*>(lParam);
            if (edit->item.iItem >= 0 && edit->item.pszText != nullptr &&
                edit->item.iItem < static_cast<int>(playlists.size()) &&
                HasVisibleName(edit->item.pszText))
            {
                selectedPlaylistIndex = edit->item.iItem;
                playlists[static_cast<std::size_t>(edit->item.iItem)].name =
                    edit->item.pszText;
                playlists[static_cast<std::size_t>(edit->item.iItem)].isModified =
                    true;
                MarkAppStateDirty();
                PostMessageW(window, MessageRefreshPlaylistList, 0, 0);
            }
            return FALSE;
        }

        if (header->code == LVN_KEYDOWN)
        {
            NMLVKEYDOWN* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
            if (key->wVKey == VK_F2)
            {
                RenameSelectedPlaylist();
            }
            else if (key->wVKey == VK_DELETE)
            {
                DeleteSelectedPlaylist(window);
            }
            return 0;
        }
        break;
    }

    case MessageRefreshPlaylistList:
        RefreshPlaylistList(playlistListView, playlists);
        return 0;

    case WM_CONTEXTMENU:
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
                        L"The application state could not be saved.",
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
        RECT splitterRect{splitterX, 0, splitterX + SplitterWidth, client.bottom};
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
        mainWindow = nullptr;
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
                    L"Saved application state could not be loaded.\n"
                    L"A new session will be started.",
                    WindowTitle, MB_OK | MB_ICONWARNING);
    }
    KeepSavedWindowPositionOnScreen();

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&commonControls))
    {
        MessageBoxW(nullptr, L"Common Controls の初期化に失敗しました。",
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.lpszClassName = WindowClassName;

    if (!RegisterClassExW(&windowClass))
    {
        MessageBoxW(nullptr, L"ウインドウクラスの登録に失敗しました。",
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
        MessageBoxW(nullptr, L"メインウインドウの作成に失敗しました。",
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
