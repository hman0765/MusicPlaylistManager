#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <string>
#include <vector>

#include "playlist.h"

namespace
{
constexpr wchar_t WindowClassName[] = L"MusicPlaylistManagerWindow";
constexpr wchar_t WindowTitle[] = L"Music Playlist Manager";
constexpr int SplitterWidth = 6;
constexpr int MinimumPaneWidth = 120;

HWND playlistListView = nullptr;
HWND trackListView = nullptr;
int splitterX = 240;
bool isDraggingSplitter = false;
int splitterDragOffset = 0;

Playlist currentPlaylist{L"New Playlist", L"", {}, false};

void InsertColumn(HWND listView, const wchar_t* heading, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    column.pszText = const_cast<wchar_t*>(heading);
    column.cx = width;
    column.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(listView, 0, &column);
}

void InsertListItem(HWND listView, int index, const std::wstring& text)
{
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<wchar_t*>(text.c_str());
    ListView_InsertItem(listView, &item);
}

void RefreshPlaylistList()
{
    ListView_DeleteAllItems(playlistListView);
    InsertListItem(playlistListView, 0, currentPlaylist.name);
    ListView_SetItemState(playlistListView, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
}

void RefreshTrackList()
{
    ListView_DeleteAllItems(trackListView);
    for (std::size_t index = 0; index < currentPlaylist.tracks.size(); ++index)
    {
        InsertListItem(trackListView, static_cast<int>(index),
                       currentPlaylist.tracks[index].path);
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
    ListView_SetColumnWidth(trackListView, 0, std::max(0, rightWidth - 4));
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
                AddTrack(currentPlaylist, path);
            }
        }
        RefreshTrackList();
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
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
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
        InsertColumn(playlistListView, L"Playlist", splitterX);
        InsertColumn(trackListView, L"Tracks", 400);
        RefreshPlaylistList();
        RefreshTrackList();
        DragAcceptFiles(window, TRUE);
        return 0;

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
