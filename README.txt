Playlist Manager
Version 1.02

Japanese documentation: README_ja.txt

OVERVIEW

Playlist Manager is a Windows 11 application for managing music playlists
and exporting M3U8 playlists. It uses a native Windows interface and keeps
playlist data separate from the audio files on disk.

LINKS

Official Website:
https://app2026kak.netlify.app/playlist-manager/

Online Manual:
https://app2026kak.netlify.app/playlist-manager/manual

Source Code:
https://github.com/hman0765/MusicPlaylistManager

MAIN FEATURES

- Playlist and playlist group management
- M3U8 and M3U playlist import
- M3U8 playlist export
- Audio file drag and drop from Windows Explorer
- Metadata retrieval using TagLib
- Metadata retrieval progress display
- Customizable track columns
- Preset and custom EXTINF formatting
- Send selected tracks or their folder to external applications
- Track count and total duration display

BASIC OPERATION

Change the UI language
  Use Settings > Language and choose English or Japanese. The selection is
  saved immediately and takes effect the next time Playlist Manager starts.
  The interface is not rebuilt while the application is running.

Create a playlist
  Use Playlist > New Playlist. Playlist names can be renamed from the
  Playlist menu or with F2.

Add audio files
  Drag supported audio files from Windows Explorer onto the Track pane, or
  use File > Open Audio Files.

Import an M3U8 or M3U playlist
  Use File > Import Playlist, or drop an M3U8 or M3U file onto the
  application. M3U files support UTF-8 and CP932 text. Percent-encoded UTF-8
  paths produced by some players are decoded when possible.

Export an M3U8 playlist
  Select a playlist and use File > Export M3U8. EXTINF text formatting can
  be configured under Settings > EXTINF Format.

Get Metadata
  Select one or more tracks and use Track > Get Metadata. Only metadata for
  currently visible track columns is retrieved. This reads information from
  the audio files; it does not write or change audio file tags. A modal
  progress window shows the number of completed and selected tracks, then
  closes automatically when processing finishes. Missing or unreadable files
  are counted as processed so the remaining tracks can continue.

Organize playlists
  Use Playlist > Organize Playlists to create groups, move playlists between
  groups, reorder items, or remove playlists.

SEND TO APPLICATIONS

Use Settings > Send To Applications to register an external player, tag
editor, or other executable. Applications can be reordered with the Up and
Down buttons. The first application in the list is the default destination
used when a track is double-clicked.

The Arguments field must contain exactly one placeholder:

  %files%   Replaced with the selected audio file paths. Multiple selected
            tracks can be passed to the application.

  %folder%  Replaced with the parent folder of one selected track. This mode
            is available only when exactly one track is sent.

Example:

  /add %files%

DATA LOCATION

Application state and settings are saved automatically in:

  %LOCALAPPDATA%\PlaylistManager\state.json

IMPORTANT FILE SAFETY INFORMATION

Deleting a playlist or removing a track from Playlist Manager does not delete
the actual audio file from disk. Playlist Manager changes only its internal
playlist data and exported playlist files.

Get Metadata reads tags and audio properties. Version 1.02 does not edit or
write tags to audio files.

SYSTEM REQUIREMENTS

- Windows 11
- No additional TagLib DLL is required; the application is distributed as a
  statically linked executable.

LICENSE

This project is licensed under the MIT License.
See LICENSE for details.

For the Japanese version of this document, see README_ja.txt.
