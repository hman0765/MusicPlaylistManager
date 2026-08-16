#pragma once

#include <string>

enum class AppLanguage
{
    English,
    Japanese
};

enum class UiText
{
    FileMenu, PlaylistMenu, TrackMenu, SettingsMenu, HelpMenu, LanguageMenu,
    EnglishLanguage, JapaneseLanguage,
    OpenAudioFiles, ImportPlaylist, ExportM3U8, Exit,
    NewPlaylist, RenamePlaylist, DeletePlaylist, OrganizePlaylists,
    GetMetadata, OpenInExplorer, Properties, SendTo, Delete,
    SendToApplications, Columns, ExtinfFormat, AboutPlaylistManager,
    PlaylistHeader, TrackColumnsTitle,
    TitleColumn, ArtistColumn, AlbumColumn, DurationColumn, CommentColumn,
    PathColumn, TrackNumberColumn, YearColumn, GenreColumn, AlbumArtistColumn,
    DiscNumberColumn, FormatColumn, BitrateColumn, SampleRateColumn,
    FileSizeColumn, DateModifiedColumn,
    SelectedStatus, PlaylistStatus, TracksStatus,
    PlaylistOrganizerTitle, GroupsLabel, PlaylistsLabel, GroupHeader,
    NewGroup, RenameGroup, DeleteGroup, MoveTo,
    SendToWindowTitle, Add, Edit, Remove, Close, Name, Executable, Arguments,
    Browse, Ok, Cancel, AddSendToApplication, EditSendToApplication,
    TrackColumnsWindowTitle, ColumnHeader,
    ExtinfWindowTitle, Preset, ArtistTitlePreset, TitlePreset,
    ArtistTitleAlbumPreset, Custom, Preview,
    CustomExtinfWindowTitle, FieldsHelpOne, FieldsHelpTwo,
    AboutBodyPrefix, AboutBodySuffix, AboutTitle,
    LanguageSaved, LanguageSaveFailed,
    NoPlaylistSelected, SaveDialogFailed, ExportSucceeded, ExportFailed,
    OpenDialogFailed, CreatePlaylistFirst, StateSaveFailed,
    NameRequired, ApplicationNamesUnique, ExecutableRequired,
    ExecutableMustExist, ArgumentsRequired, ArgumentsPlaceholderRequired,
    ApplicationLimit, SendFailedPrefix, SendFailedSuffix,
    SelectedTracksExpandFailed, ConfiguredExecutableMissing,
    TooManyTracks, StateLoadFailed,
    CommonControlsFailed, WindowClassFailed, MainWindowFailed,
    AudioFilesFilter, M3U8FilesFilter, OpenAudioFilesTitle,
    ImportPlaylistTitle, ExportPlaylistTitle,
    DeletePlaylistQuestionPrefix, DeletePlaylistQuestionSuffix,
    AudioFilesNotDeleted, DeleteGroupQuestionPrefix, DeleteGroupQuestionSuffix,
    DeletePlaylistsQuestionPrefix, DeletePlaylistsQuestionSuffix,
    GroupNamesUnique,
    NewGroupReserved, LoadNewPlaylistPrompt, LoadPlaylistPrompt,
    LoadPlaylistOptions, LoadM3U8Failed, RemoveSendToPrefix,
    RemoveSendToSuffix, ApplicationsFilter, SelectExecutableTitle,
    InvalidExtinfFormat, WindowsErrorPrefix,
    Count
};

const wchar_t* GetUiText(UiText id, AppLanguage language);
const wchar_t* GetAppLanguageCode(AppLanguage language);
bool TryParseAppLanguage(const std::wstring& code, AppLanguage& language);
