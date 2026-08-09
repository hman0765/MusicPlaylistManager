#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#include "playlist.h"

namespace
{
constexpr wchar_t WindowClassName[] = L"MusicPlaylistManagerWindow";
constexpr wchar_t WindowTitle[] = L"Music Playlist Manager";
constexpr int SplitterWidth = 6;
constexpr int MinimumPaneWidth = 120;
constexpr int TitleColumnWidth = 180;
constexpr int ArtistColumnWidth = 140;
constexpr int AlbumColumnWidth = 160;
constexpr int DurationColumnWidth = 85;
constexpr int MinimumPathColumnWidth = 240;
constexpr UINT CommandNewPlaylist = 1001;
constexpr UINT CommandRenamePlaylist = 1002;
constexpr UINT CommandDeletePlaylist = 1003;
constexpr UINT MessageRefreshPlaylistList = WM_APP + 1;

HWND playlistListView = nullptr;
HWND trackListView = nullptr;
int splitterX = 240;
bool isDraggingSplitter = false;
int splitterDragOffset = 0;

std::vector<Playlist> playlists{{L"New Playlist", L"", {}, false}};
int selectedPlaylistIndex = 0;
bool isRefreshingPlaylistList = false;

class ComApartment
{
public:
    ComApartment()
        : initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
    }

    ~ComApartment()
    {
        if (initialized)
        {
            CoUninitialize();
        }
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

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
        InsertListItem(listView, row, track.title);
        SetListItemText(listView, row, 1, track.artist);
        SetListItemText(listView, row, 2, track.album);
        SetListItemText(listView, row, 3, track.duration);
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

    RefreshPlaylistList(playlistListView, playlists);
    RefreshSelectedTrackList();
}

void SelectPlaylist(int index)
{
    if (index < 0 || index >= static_cast<int>(playlists.size()))
    {
        return;
    }

    selectedPlaylistIndex = index;
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
    const int fixedTrackColumnsWidth = TitleColumnWidth + ArtistColumnWidth +
                                       AlbumColumnWidth + DurationColumnWidth;
    ListView_SetColumnWidth(
        trackListView, 4,
        std::max(MinimumPathColumnWidth,
                 rightWidth - fixedTrackColumnsWidth - 4));
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

void HandleDroppedFiles(HWND window, HDROP drop)
{
    POINT dropPoint{};
    const bool droppedInTrackPane =
        DragQueryPoint(drop, &dropPoint) != FALSE &&
        IsPointInTrackPane(window, dropPoint);

    if (droppedInTrackPane)
    {
        Playlist* selectedPlaylist = GetSelectedPlaylist();
        if (selectedPlaylist == nullptr)
        {
            MessageBoxW(window,
                        L"No playlist selected. Create a playlist first.",
                        WindowTitle, MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT index = 0; index < fileCount; ++index)
            {
                const UINT length = DragQueryFileW(drop, index, nullptr, 0);
                std::vector<wchar_t> buffer(length + 1);
                DragQueryFileW(drop, index, buffer.data(),
                               static_cast<UINT>(buffer.size()));

                const std::wstring path(buffer.data());
                if (IsSupportedAudioPath(path))
                {
                    AddTrack(*selectedPlaylist, CreateTrackFromFile(path));
                }
            }
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
        InsertColumn(trackListView, 0, L"Title", TitleColumnWidth);
        InsertColumn(trackListView, 1, L"Artist", ArtistColumnWidth);
        InsertColumn(trackListView, 2, L"Album", AlbumColumnWidth);
        InsertColumn(trackListView, 3, L"Duration", DurationColumnWidth);
        InsertColumn(trackListView, 4, L"Path", MinimumPathColumnWidth);
        RefreshPlaylistList(playlistListView, playlists);
        RefreshSelectedTrackList();
        DragAcceptFiles(window, TRUE);
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
        }
        break;

    case WM_NOTIFY:
    {
        NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
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
                selectedPlaylistIndex = change->iItem;
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
        break;

    case WM_SIZE:
        LayoutChildren(window);
        return 0;

    case WM_LBUTTONDOWN:
    {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsPointOnSplitter(window, point))
        {
            isDraggingSplitter = true;
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
        }
        return 0;

    case WM_CAPTURECHANGED:
        isDraggingSplitter = false;
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
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {360, 240};
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    ComApartment comApartment;

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
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        MessageBoxW(nullptr, L"メインウインドウの作成に失敗しました。",
                    WindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
