// Copyright (c) 2014, Alexey Ivanov

#include "stdafx.h"
#include "utils.h"
#include "../aimp/manager_impl_common.h"

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

void setCurrentMuteMode(bool value, Rpc::Value& result)
{
    result["mute_mode_on"] = value;
}

void setCurrentRepeatMode(bool value, Rpc::Value& result)
{
    result["repeat_mode_on"] = value;
}

void setCurrentShuffleMode(bool value, Rpc::Value& result)
{
    result["shuffle_mode_on"] = value;
}

void setCurrentRadioCaptureMode(bool value, Rpc::Value& result)
{
    result["radio_capture_mode_on"] = value;
}

void setIsCurrentTrackSourceRadioIfPossible(const AIMPManager& aimp_manager, Rpc::Value& result)
{
    if (aimp_manager.getPlaybackState() != AIMPManager::STOPPED) {
        TrackDescription playing_track = aimp_manager.getPlayingTrack();
        result["current_track_source_radio"] = aimp_manager.getTrackSourceType(playing_track) == AIMPManager::SOURCE_TYPE_RADIO;
    }
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
    setCurrentVolume          (aimp_manager.getStatus(AIMPManager::STATUS_VOLUME),             result);
    setCurrentMuteMode        (aimp_manager.getStatus(AIMPManager::STATUS_MUTE) != 0,          result);
    setCurrentRepeatMode      (aimp_manager.getStatus(AIMPManager::STATUS_REPEAT) != 0,        result);
    setCurrentShuffleMode     (aimp_manager.getStatus(AIMPManager::STATUS_SHUFFLE) != 0,       result);
    setCurrentRadioCaptureMode(aimp_manager.getStatus(AIMPManager::STATUS_RADIO_CAPTURE) != 0, result);
    setIsCurrentTrackSourceRadioIfPossible(aimp_manager, result);
}

void setPlaylistsContentChangeInfo(const AIMPPlayer::AIMPManager& aimp_manager, Rpc::Value& result)
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT id, crc32 FROM Playlists";

    sqlite3* db = AIMPPlayer::getPlaylistsDB(aimp_manager);
    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    result["playlists_changed"] = true;
    Rpc::Value& playlists_rpc = result["playlists"];

    for(int playlist_index = 0; ; ++playlist_index) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            playlists_rpc.setSize(playlist_index + 1);
            Rpc::Value& playlist_rpc = playlists_rpc[playlist_index];
            const PlaylistID id = sqlite3_column_int(stmt, 0);
            crc32_t crc32       = sqlite3_column_int(stmt, 1);
            playlist_rpc["id"] = id;
            playlist_rpc["crc32"] = static_cast<int>(crc32); // static_cast<int>( aimp_manager.getPlaylistCRC32(id) ); 
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }    
}

const std::string& getStringFieldID(PlaylistEntry::FIELD_IDs id)
{
    // Notice, order and count of should be syncronized with PlaylistEntry::FIELD_IDs.
    static std::string fields_ids[] = {"id", "album", "artist", "date", "filename", "genre", "title", "bitrate", "channels_count", "duration", "filesize", "rating", "samplerate" };
    //enum PlaylistEntry::FIELD_IDs   { ID,  ALBUM,   ARTIST,   DATE,   FILENAME,   GENRE,   TITLE,   BITRATE,   CHANNELS_COUNT,   DURATION,   FILESIZE,   RATING,    SAMPLE_RATE }
    
    Utilities::AssertArraySize<PlaylistEntry::FIELDS_COUNT>(fields_ids);
    assert(0 <= id && id < PlaylistEntry::FIELDS_COUNT);
    return fields_ids[id];
}

const std::string& getStringFieldID(Playlist::FIELD_IDs id)
{
    // Notice, order and count of should be syncronized with Playlist::FIELD_IDs.
    static std::string fields_ids[] = { "id", "title", "entries_count", "duration", "size_of_entries", "crc32" };
    //enum Playlist::FIELD_IDs        { ID,   TITLE,   ENTRIES_COUNT,   DURATION,   SIZE_OF_ALL_ENTRIES_IN_BYTES, CRC32, FIELDS_COUNT };
    Utilities::AssertArraySize<Playlist::FIELDS_COUNT>(fields_ids);
    assert(0 <= id && id < Playlist::FIELDS_COUNT);
    return fields_ids[id];
}

} // namespace AimpRpcMethods::RpcResultUtils

} // namespace AimpRpcMethods
