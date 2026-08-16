#include "ui_text.h"

#include <array>
#include <cstddef>

namespace
{
struct Translation
{
    const wchar_t* english;
    const wchar_t* japanese;
};

constexpr std::array<Translation, static_cast<std::size_t>(UiText::Count)>
Translations{{
    {L"&File", L"ファイル"}, {L"&Playlist", L"プレイリスト"},
    {L"&Track", L"トラック"}, {L"&Settings", L"設定"},
    {L"&Help", L"ヘルプ"}, {L"Language", L"言語"},
    {L"English", L"English"}, {L"日本語", L"日本語"},
    {L"Open Audio Files...", L"音声ファイルを開く..."},
    {L"Import Playlist...", L"プレイリストをインポート..."},
    {L"Export M3U8...", L"M3U8をエクスポート..."},
    {L"Exit", L"終了"},
    {L"New Playlist", L"新しいプレイリスト"},
    {L"Rename Playlist", L"プレイリスト名を変更"},
    {L"Delete Playlist", L"プレイリストを削除"},
    {L"Organize Playlists...", L"プレイリストを整理..."},
    {L"Get Metadata", L"メタデータを取得"},
    {L"Open in Explorer", L"エクスプローラーで開く"},
    {L"Properties", L"プロパティ"}, {L"Send to", L"アプリに送る"},
    {L"Delete", L"削除"},
    {L"Send To Applications...", L"アプリに送る..."},
    {L"Columns...", L"カラム..."}, {L"EXTINF Format...", L"EXTINF形式..."},
    {L"About Playlist Manager...", L"Playlist Managerについて..."},
    {L"Playlist", L"プレイリスト"}, {L"Track Columns", L"トラックカラム"},
    {L"Title", L"タイトル"}, {L"Artist", L"アーティスト"},
    {L"Album", L"アルバム"}, {L"Duration", L"再生時間"},
    {L"Comment", L"コメント"}, {L"Path", L"パス"},
    {L"Track Number", L"トラック番号"}, {L"Year", L"年"},
    {L"Genre", L"ジャンル"}, {L"Album Artist", L"アルバムアーティスト"},
    {L"Disc Number", L"ディスク番号"}, {L"Format", L"形式"},
    {L"Bitrate", L"ビットレート"}, {L"Sample Rate", L"サンプリングレート"},
    {L"File Size", L"ファイルサイズ"}, {L"Date Modified", L"更新日時"},
    {L"Selected: ", L"選択: "}, {L"Playlist: ", L"プレイリスト: "},
    {L" tracks / ", L"曲 / "},
    {L"Playlist Organizer", L"プレイリスト整理"},
    {L"Groups", L"グループ"}, {L"Playlists", L"プレイリスト"},
    {L"Group", L"グループ"}, {L"New Group", L"新しいグループ"},
    {L"Rename Group", L"グループ名を変更"},
    {L"Delete Group", L"グループを削除"}, {L"Move to", L"移動先"},
    {L"Send To Applications", L"アプリに送る"}, {L"Add", L"追加"},
    {L"Edit", L"編集"}, {L"Remove", L"削除"}, {L"Close", L"閉じる"},
    {L"Name", L"名前"}, {L"Executable", L"実行ファイル"},
    {L"Arguments", L"引数"}, {L"Browse...", L"参照..."},
    {L"OK", L"OK"}, {L"Cancel", L"キャンセル"},
    {L"Add Send To Application", L"アプリに送る: 追加"},
    {L"Edit Send To Application", L"アプリに送る: 編集"},
    {L"Track Columns", L"トラックカラム"}, {L"Column", L"カラム"},
    {L"EXTINF Format", L"EXTINF形式"}, {L"Preset:", L"プリセット:"},
    {L"Artist - Title", L"アーティスト - タイトル"},
    {L"Title", L"タイトル"},
    {L"Artist - Title - Album", L"アーティスト - タイトル - アルバム"},
    {L"Custom", L"カスタム"}, {L"Preview:", L"プレビュー:"},
    {L"Custom EXTINF Format", L"カスタムEXTINF形式"},
    {L"Fields: {title}, {artist}, {album}, {comment}, {tracknumber}, {year}, {genre}",
     L"フィールド: {title}, {artist}, {album}, {comment}, {tracknumber}, {year}, {genre}"},
    {L"{albumartist}, {discnumber}, {format}, {bitrate}, {samplerate}, {filesize}, {datemodified}",
     L"{albumartist}, {discnumber}, {format}, {bitrate}, {samplerate}, {filesize}, {datemodified}"},
    {L"Playlist Manager\n\nVersion ", L"Playlist Manager\n\nバージョン "},
    {L"\n\nA playlist management application for Windows.",
     L"\n\nWindows用プレイリスト管理アプリケーション"},
    {L"About Playlist Manager", L"Playlist Managerについて"},
    {L"Language setting saved.\nThe new language will be used after restarting Playlist Manager.",
     L"言語設定を保存しました。\nPlaylist Managerを再起動すると新しい言語が適用されます。"},
    {L"The language setting could not be saved.", L"言語設定を保存できませんでした。"},
    {L"No playlist selected.", L"プレイリストが選択されていません。"},
    {L"The Save dialog could not be opened.", L"保存ダイアログを開けませんでした。"},
    {L"The playlist was exported successfully.", L"プレイリストをエクスポートしました。"},
    {L"Failed to export the m3u8 playlist.", L"M3U8プレイリストのエクスポートに失敗しました。"},
    {L"The Open dialog could not be opened.", L"開くダイアログを表示できませんでした。"},
    {L"No playlist selected. Create a playlist first.", L"プレイリストがありません。先にプレイリストを作成してください。"},
    {L"The application state could not be saved.", L"アプリの状態を保存できませんでした。"},
    {L"Name is required.", L"名前を入力してください。"},
    {L"Application names must be unique.", L"アプリ名は重複できません。"},
    {L"Executable is required.", L"実行ファイルを指定してください。"},
    {L"Executable must be an existing file.", L"存在する実行ファイルを指定してください。"},
    {L"Arguments are required.", L"引数を入力してください。"},
    {L"Arguments must contain exactly one of:\n%files%\n%folder%",
     L"引数には次のどちらか1つを指定してください:\n%files%\n%folder%"},
    {L"Up to 15 applications can be registered.", L"登録できるアプリは15件までです。"},
    {L"Failed to send tracks to \"", L"「"}, {L"\".", L"」への送信に失敗しました。"},
    {L"The selected tracks could not be expanded.", L"選択したトラックを引数へ展開できませんでした。"},
    {L"The configured executable does not exist.", L"設定された実行ファイルが存在しません。"},
    {L"Too many tracks to send in one command.", L"一度に送るトラックが多すぎます。"},
    {L"Saved application state could not be loaded.\nA new session will be started.",
     L"保存されたアプリの状態を読み込めませんでした。\n新しいセッションを開始します。"},
    {L"Common Controls could not be initialized.", L"Common Controlsの初期化に失敗しました。"},
    {L"The window class could not be registered.", L"ウィンドウクラスを登録できませんでした。"},
    {L"The main window could not be created.", L"メインウィンドウを作成できませんでした。"},
    {L"Audio Files (*.mp3;*.flac;*.wav;*.m4a;*.ogg)\0*.mp3;*.flac;*.wav;*.m4a;*.ogg\0All Files (*.*)\0*.*\0\0",
     L"音声ファイル (*.mp3;*.flac;*.wav;*.m4a;*.ogg)\0*.mp3;*.flac;*.wav;*.m4a;*.ogg\0すべてのファイル (*.*)\0*.*\0\0"},
    {L"M3U8 Playlists (*.m3u8)\0*.m3u8\0All Files (*.*)\0*.*\0\0",
     L"M3U8プレイリスト (*.m3u8)\0*.m3u8\0すべてのファイル (*.*)\0*.*\0\0"},
    {L"Open Audio Files", L"音声ファイルを開く"},
    {L"Import Playlist", L"プレイリストをインポート"},
    {L"Export Playlist as M3U8", L"プレイリストをM3U8としてエクスポート"},
    {L"Delete playlist \"", L"プレイリスト「"}, {L"\"?", L"」を削除しますか？"},
    {L"Audio files will not be deleted.", L"音声ファイル自体は削除されません。"},
    {L"Delete group \"", L"グループ「"}, {L"\"?", L"」を削除しますか？"},
    {L"Delete ", L""},
    {L" playlists?", L"個のプレイリストを削除しますか？"},
    {L"Group names must be non-empty and unique.", L"グループ名は空欄にできず、重複もできません。"},
    {L"The New group cannot be renamed.", L"「新規」グループの名前は変更できません。"},
    {L"No playlist is selected.\n\nLoad this M3U8 as a new playlist?\n\n",
     L"プレイリストが選択されていません。\n\nこのM3U8を新しいプレイリストとして読み込みますか？\n\n"},
    {L"Load this M3U8 playlist?\n\n",
     L"このM3U8プレイリストを読み込みますか？\n\n"},
    {L"\n\nYes: Add as a new playlist\nNo: Append to the current playlist\nCancel: Do nothing",
     L"\n\nはい: 新しいプレイリストとして追加\nいいえ: 現在のプレイリストへ追加\nキャンセル: 何もしない"},
    {L"Failed to load the M3U8 file.\nThe file must be readable UTF-8 text.",
     L"M3U8ファイルを読み込めませんでした。\n読み取り可能なUTF-8テキストである必要があります。"},
    {L"Remove \"", L"「"},
    {L"\" from Send To applications?", L"」をアプリに送る一覧から削除しますか？"},
    {L"Applications (*.exe)\0*.exe\0All Files (*.*)\0*.*\0\0",
     L"アプリケーション (*.exe)\0*.exe\0すべてのファイル (*.*)\0*.*\0\0"},
    {L"Select Application", L"アプリケーションを選択"},
    {L"The EXTINF format is invalid.", L"EXTINF形式が正しくありません。"},
    {L"Windows error ", L"Windowsエラー "}
}};
}

const wchar_t* GetUiText(UiText id, AppLanguage language)
{
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= Translations.size())
        return L"";
    return language == AppLanguage::Japanese
        ? Translations[index].japanese : Translations[index].english;
}

const wchar_t* GetAppLanguageCode(AppLanguage language)
{
    return language == AppLanguage::Japanese ? L"ja" : L"en";
}

bool TryParseAppLanguage(const std::wstring& code, AppLanguage& language)
{
    if (code == L"en")
    {
        language = AppLanguage::English;
        return true;
    }
    if (code == L"ja")
    {
        language = AppLanguage::Japanese;
        return true;
    }
    language = AppLanguage::English;
    return false;
}
