// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "../utils/scope_guard.h"
#include "../utils/sqlite_util.h"
#include "manager2.6.h"
#include "manager3.0.h"

namespace AIMPPlayer
{

inline const char* asString(AIMPManager::STATUS status)
{
 switch (status) {
    case AIMPManager::STATUS_VOLUME:    return "VOLUME";
    case AIMPManager::STATUS_BALANCE:   return "BALANCE";
    case AIMPManager::STATUS_SPEED:     return "SPEED";
    case AIMPManager::STATUS_Player:    return "Player";
    case AIMPManager::STATUS_MUTE:      return "MUTE";
    case AIMPManager::STATUS_REVERB:    return "REVERB";
    case AIMPManager::STATUS_ECHO:      return "ECHO";
    case AIMPManager::STATUS_CHORUS:    return "CHORUS";
    case AIMPManager::STATUS_Flanger:   return "Flanger";

    case AIMPManager::STATUS_EQ_STS:    return "EQ_STS";
    case AIMPManager::STATUS_EQ_SLDR01: return "EQ_SLDR01";
    case AIMPManager::STATUS_EQ_SLDR02: return "EQ_SLDR02";
    case AIMPManager::STATUS_EQ_SLDR03: return "EQ_SLDR03";
    case AIMPManager::STATUS_EQ_SLDR04: return "EQ_SLDR04";
    case AIMPManager::STATUS_EQ_SLDR05: return "EQ_SLDR05";
    case AIMPManager::STATUS_EQ_SLDR06: return "EQ_SLDR06";
    case AIMPManager::STATUS_EQ_SLDR07: return "EQ_SLDR07";
    case AIMPManager::STATUS_EQ_SLDR08: return "EQ_SLDR08";
    case AIMPManager::STATUS_EQ_SLDR09: return "EQ_SLDR09";
    case AIMPManager::STATUS_EQ_SLDR10: return "EQ_SLDR10";
    case AIMPManager::STATUS_EQ_SLDR11: return "EQ_SLDR11";
    case AIMPManager::STATUS_EQ_SLDR12: return "EQ_SLDR12";
    case AIMPManager::STATUS_EQ_SLDR13: return "EQ_SLDR13";
    case AIMPManager::STATUS_EQ_SLDR14: return "EQ_SLDR14";
    case AIMPManager::STATUS_EQ_SLDR15: return "EQ_SLDR15";
    case AIMPManager::STATUS_EQ_SLDR16: return "EQ_SLDR16";
    case AIMPManager::STATUS_EQ_SLDR17: return "EQ_SLDR17";
    case AIMPManager::STATUS_EQ_SLDR18: return "EQ_SLDR18";

    case AIMPManager::STATUS_REPEAT:    return "REPEAT";
    case AIMPManager::STATUS_STAY_ON_TOP: return "STAY_ON_TOP";
    case AIMPManager::STATUS_POS:       return "POS";
    case AIMPManager::STATUS_LENGTH:    return "LENGTH";
    case AIMPManager::STATUS_ACTION_ON_END_OF_PLAYLIST: return "ACTION_ON_END_OF_PLAYLIST";
    case AIMPManager::STATUS_REPEAT_SINGLE_FILE_PLAYLISTS: return "REPEAT_SINGLE_FILE_PLAYLISTS";
    case AIMPManager::STATUS_KBPS:      return "KBPS";
    case AIMPManager::STATUS_KHZ:       return "KHZ";
    case AIMPManager::STATUS_MODE:      return "MODE";
    case AIMPManager::STATUS_RADIO_CAPTURE: return "RADIO_CAPTURE";
    case AIMPManager::STATUS_STREAM_TYPE: return "STREAM_TYPE";
    case AIMPManager::STATUS_REVERSETIME: return "REVERSETIME";
    case AIMPManager::STATUS_SHUFFLE:   return "SHUFFLE";
    case AIMPManager::STATUS_MAIN_HWND: return "MAIN_HWND";
    case AIMPManager::STATUS_TC_HWND:   return "TC_HWND";
    case AIMPManager::STATUS_APP_HWND:  return "APP_HWND";
    case AIMPManager::STATUS_PL_HWND:   return "PL_HWND";
    case AIMPManager::STATUS_EQ_HWND:   return "EQ_HWND";
    case AIMPManager::STATUS_TRAY:      return "TRAY";
    default:
        break;
    }

    assert(!"unknown status in "__FUNCTION__);
    static std::string status_string;
    std::stringstream os;
    os << static_cast<int>(status);
    status_string = os.str();
    return status_string.c_str();
}

inline size_t getEntriesCountDB(PlaylistID playlist_id, sqlite3* playlists_db)
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT COUNT(*) FROM PlaylistsEntries WHERE playlist_id=" << playlist_id;

    sqlite3_stmt* stmt = createStmt( playlists_db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    size_t total_entries_count = 0;

	const int rc_db = sqlite3_step(stmt);
    if (SQLITE_ROW == rc_db) {
        assert(sqlite3_column_count(stmt) > 0);
        total_entries_count = sqlite3_column_int(stmt, 0);
    } else {
        const std::string msg = MakeString() << "sqlite3_step() error "
                                             << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                             << ". Query: " << query.str();
        throw std::runtime_error(msg);
    }

    return total_entries_count;
}

inline sqlite3* getPlaylistsDB(const AIMPPlayer::AIMPManager& aimp_manager) {
    if (       const AIMPPlayer::AIMPManager30* mgr3 = dynamic_cast<const AIMPPlayer::AIMPManager30*>(&aimp_manager) ) {
        return mgr3->playlists_db();
    } else if (const AIMPPlayer::AIMPManager26* mgr2 = dynamic_cast<const AIMPPlayer::AIMPManager26*>(&aimp_manager) ) {
        return mgr2->playlists_db();
    } else {
        using namespace Utilities;
        const std::string msg = MakeString() << __FUNCTION__ ": invalid AIMPManager object. AIMPManager30 and AIMPManager26 are only supported.";
        throw std::runtime_error(msg);
    }
}

} // namespace AIMPPlayer
