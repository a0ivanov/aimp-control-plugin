// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "utils.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Rpc::RequestHandler>(); }
}

namespace AimpRpcMethods {
namespace RpcResultUtils {

using namespace AIMPPlayer;

// for use inside setCurrentPlaybackState() function.
struct mapping_playback_state_to_string
{
  AIMPManager::PLAYBACK_STATE internal_playback_state;
  const char* rpc_playback_state;
} const mappings_playback_state_to_string[] =
{
  { AIMPManager::PLAYING,   "playing" },
  { AIMPManager::STOPPED,   "stopped" },
  { AIMPManager::PAUSED,    "paused" },
  { AIMPManager::PLAYBACK_STATES_COUNT, 0 } // Marks end of list.
};

void setCurrentPlaybackState(AIMPManager::PLAYBACK_STATE playback_state, Rpc::Value& result)
{
    for (const mapping_playback_state_to_string* m = mappings_playback_state_to_string;
         m->internal_playback_state != AIMPManager::PLAYBACK_STATES_COUNT;
         ++m)
    {
        if (m->internal_playback_state == playback_state) {
            result["playback_state"] = m->rpc_playback_state;
            return;
        }
    }

    BOOST_LOG_SEV(logger(), error) << "Unknown playback state " << playback_state << " in "__FUNCTION__;
    assert(!"Unknown playback state in "__FUNCTION__);
    result["playback_state"] = "unknown";
}

void setCurrentTrackProgress(size_t current_position, size_t track_length, Rpc::Value& result)
{
    result["track_position"] = current_position;
    result["track_length"] = track_length;
}

void setCurrentTrackProgressIfPossible(const AIMPManager& aimp_manager, Rpc::Value& result)
{
    if (aimp_manager.getPlaybackState() != AIMPManager::STOPPED) {
        const int track_position = aimp_manager.getStatus(AIMPManager::STATUS_POS),
                  track_length   = aimp_manager.getStatus(AIMPManager::STATUS_LENGTH);
        if (   track_position >= 0 
            && track_length > 0 // track_length == 0 for internet radio.
            )
        {
            setCurrentTrackProgress(track_position, track_length, result);
        }
    }
}

void setCurrentPlaylist(PlaylistID playlist_id, Rpc::Value& result)
{
    result["playlist_id"] = playlist_id;
}

void setCurrentPlaylistEntry(PlaylistEntryID track_id, Rpc::Value& result)
{
    result["track_id"] = track_id;
}

void setCurrentVolume(int volume, Rpc::Value& result)
{
    result["volume"] = volume;
}

void setCurrentMuteMode(bool mute_on, Rpc::Value& result)
{
    result["mute_mode_on"] = mute_on;
}

void setCurrentRepeatMode(bool repeat_on, Rpc::Value& result)
{
    result["repeat_mode_on"] = repeat_on;
}

void setCurrentShuffleMode(bool shuffle_on, Rpc::Value& result)
{
    result["shuffle_mode_on"] = shuffle_on;
}

void setCurrentPlayingSourceInfo(const AIMPManager& aimp_manager, Rpc::Value& result)
{
    setCurrentPlaylist(aimp_manager.getPlayingPlaylist(), result);
    setCurrentPlaylistEntry(aimp_manager.getPlayingEntry(), result);
}

void setControlPanelInfo(const AIMPManager& aimp_manager, Rpc::Value& result)
{
    setCurrentPlaybackState(aimp_manager.getPlaybackState(), result);
    setCurrentTrackProgressIfPossible(aimp_manager, result);
    setCurrentPlayingSourceInfo(aimp_manager, result);
    setCurrentVolume     (aimp_manager.getStatus(AIMPManager::STATUS_VOLUME),       result);
    setCurrentMuteMode   (aimp_manager.getStatus(AIMPManager::STATUS_MUTE) != 0,    result);
    setCurrentRepeatMode (aimp_manager.getStatus(AIMPManager::STATUS_REPEAT) != 0,  result);
    setCurrentShuffleMode(aimp_manager.getStatus(AIMPManager::STATUS_SHUFFLE) != 0, result);
}

void setPlaylistsContentChangeInfo(const AIMPPlayer::AIMPManager& /*aimp_manager*/, Rpc::Value& result)
{
     result["playlists_changed"] = true;
     // ??? result["playlist_id_to_reload"];
}

const std::string& getStringFieldID(PlaylistEntry::FIELD_IDs id)
{
    // Notice, order and count of should be syncronized with PlaylistEntry::FIELD_IDs.
    static std::string fields_ids[] = {"id", "title", "artist", "album", "date", "genre", "bitrate", "duration", "filename", "filesize", "rating", "internal_aimp_id", "activity_flag" };
    //enum PlaylistEntry::FIELD_IDs   { ID,   TITLE,   ARTIST,   ALBUM,   DATE,   GENRE,   BITRATE,   DURATION,   FILENAME,   FILESIZE,   RATING,   INTERNAL_AIMP_ID,   ACTIVITY_FLAG,   FIELDS_COUNT };
    Utilities::AssertArraySize<PlaylistEntry::FIELDS_COUNT>(fields_ids);
    assert(0 <= id && id < PlaylistEntry::FIELDS_COUNT);
    return fields_ids[id];
}

const std::string& getStringFieldID(Playlist::FIELD_IDs id)
{
    // Notice, order and count of should be syncronized with Playlist::FIELD_IDs.
    static std::string fields_ids[] = { "id", "title", "entries_count", "duration", "size_of_entries" };
    //enum Playlist::FIELD_IDs        { ID,   TITLE,   ENTRIES_COUNT,   DURATION,   SIZE_OF_ALL_ENTRIES_IN_BYTES, FIELDS_COUNT };
    Utilities::AssertArraySize<Playlist::FIELDS_COUNT>(fields_ids);
    assert(0 <= id && id < Playlist::FIELDS_COUNT);
    return fields_ids[id];
}

} // namespace AimpRpcMethods::RpcResultUtils

} // namespace AimpRpcMethods
