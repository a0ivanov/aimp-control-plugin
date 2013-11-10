// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "manager2.6.h"
#include "manager_impl_common.h"
#include "playlist_entry.h"
#include "aimp2_sdk.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/image.h"
#include "utils/util.h"
#include "utils/scope_guard.h"
#include "utils/sqlite_util.h"
#include "sqlite/sqlite.h"
#include <boost/assign/std.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <algorithm>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}


/* Specialization of Utilities::crc32(T) for AIMP2SDK::AIMP2FileInfo struct.
     Be sure that following assertion is ok:
        AIMP2FileInfo info;
        PlaylistEntry entry(info);
        assert( crc32(entry) == crc32(info) )
*/
template<>
crc32_t Utilities::crc32<AIMP2SDK::AIMP2FileInfo>(const AIMP2SDK::AIMP2FileInfo& info)
{
#define ENTRY_FIELD_CRC32(field) (info.n##field##Len == 0) ? 0 : Utilities::crc32( &info.s##field[0], info.n##field##Len * sizeof(info.s##field[0]) )
    const crc32_t members_crc32_list [] = {
        ENTRY_FIELD_CRC32(Album),
        ENTRY_FIELD_CRC32(Artist),
        ENTRY_FIELD_CRC32(Date),
        ENTRY_FIELD_CRC32(FileName),
        ENTRY_FIELD_CRC32(Genre),
        ENTRY_FIELD_CRC32(Title),
        info.nBitRate,
        info.nChannels,
        info.nDuration,
        Utilities::crc32(info.nFileSize),
        info.nRating,
        info.nSampleRate
    };
#undef ENTRY_FIELD_CRC32

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}


namespace AIMPPlayer
{

using namespace Utilities;
using namespace AIMP2SDK;

AIMPManager26::AIMPManager26(boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                             boost::asio::io_service& io_service_
                             )
    :
    aimp2_controller_(aimp_controller),
    strand_(io_service_),
#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
    playlists_check_timer_( io_service_, boost::posix_time::seconds(kPLAYLISTS_CHECK_PERIOD_SEC) ),
#endif
    next_listener_id_(0),
    playlists_db_(nullptr)
{
    try {
        initializeAIMPObjects();

        initPlaylistDB();

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
        // set timer which will periodically check when AIMP has load playlists.
        playlists_check_timer_.async_wait( boost::bind(&AIMPManager26::onTimerPlaylistsChangeCheck, this, _1) );
#endif
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured while AIMPManager26 initialization. Reason:") + e.what() );
    }

    registerNotifiers();
}

AIMPManager26::~AIMPManager26()
{
    unregisterNotifiers();

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
    BOOST_LOG_SEV(logger(), info) << "Stopping playlists check timer.";
    playlists_check_timer_.cancel();
    BOOST_LOG_SEV(logger(), info) << "Playlists check timer was stopped.";
#endif

    shutdownPlaylistDB();
}

void AIMPManager26::loadPlaylist(int playlist_index)
{
    const char * const error_prefix = "Error occured while extracting playlist data: ";
    
    int id;
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_ID_By_Index(playlist_index, &id) ) {
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_ID_By_Index failed");
    }

    { // handle crc32.
    auto it = playlist_crc32_list_.find(id);
    if (it == playlist_crc32_list_.end()) {
        it = playlist_crc32_list_.insert(std::make_pair(id,
                                                        PlaylistCRC32(id, playlists_db_)
                                                        )
                                         ).first;
    }
    it->second.reset_properties();
    }

    INT64 duration, size;
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_GetInfo(id, &duration, &size) ) {
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_GetInfo failed");
    }

    const size_t name_length = 256;
    WCHAR name[name_length + 1] = {0};
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_GetName(id, name, name_length) ) {    
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_GetName failed");
    }

    const int files_count = aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(id);

    { // db code
    sqlite3_stmt* stmt = createStmt(playlists_db_,
                                    "REPLACE INTO Playlists VALUES (?,?,?,?,?,?,?)"
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

#define bind(type, field_index, value)  rc_db = sqlite3_bind_##type(stmt, field_index, value); \
                                        if (SQLITE_OK != rc_db) { \
                                            const std::string msg = MakeString() << "Error sqlite3_bind_"#type << " " << rc_db; \
                                            throw std::runtime_error(msg); \
                                        }
#define bindText(field_index, text, textLength) rc_db = sqlite3_bind_text16(stmt, field_index, text, textLength * sizeof(WCHAR), SQLITE_STATIC); \
                                                if (SQLITE_OK != rc_db) { \
                                                    const std::string msg = MakeString() << "sqlite3_bind_text16" << " " << rc_db; \
                                                    throw std::runtime_error(msg); \
                                                }

    int rc_db;
    bind( int,  1, id );
    bind( int,  2, playlist_index );
    bindText(   3, name, wcslen(name) );
    bind( int,  4, files_count );
    bind(int64, 5, duration);
    bind(int64, 6, size);
    bind(int64, 7, kCRC32_UNINITIALIZED);
#undef bind
#undef bindText
    rc_db = sqlite3_step(stmt);
    if (SQLITE_DONE != rc_db) {
        const std::string msg = MakeString() << "sqlite3_step() error "
                                             << rc_db << ": " << sqlite3_errmsg(playlists_db_);
        throw std::runtime_error(msg);
    }
    }
}

PlaylistCRC32& AIMPManager26::getPlaylistCRC32Object(PlaylistID playlist_id) const // throws std::runtime_error
{
    auto it = playlist_crc32_list_.find(playlist_id);
    if (it != playlist_crc32_list_.end()) {
        return it->second;
    }
    throw std::runtime_error(MakeString() << "Playlist " << playlist_id << " was not found in "__FUNCTION__);
}


crc32_t AIMPManager26::getPlaylistCRC32(PlaylistID playlist_id) const // throws std::runtime_error
{
    return getPlaylistCRC32Object(playlist_id).crc32();
}

/*
    Helper for convinient load entry data from AIMP.
    Prepares AIMP2SDK::AIMP2FileInfo struct.
    Contains necessary string buffers that always are in clear state.
*/
class AIMP2FileInfoHelper
{
public:

    AIMP2SDK::AIMP2FileInfo& getEmptyFileInfo()
    {
        resetInfo();
        return info_;
    }

    AIMP2SDK::AIMP2FileInfo& getFileInfo()
        { return info_; }


    /* \return track ID. */
    DWORD trackID() const
        { return info_.nTrackID; }

    AIMP2SDK::AIMP2FileInfo& getFileInfoWithCorrectStringLengthsAndNonEmptyTitle()
    {
        fixStringLengths();
        fixEmptyTitle();
        return info_;
    }

private:

    void resetInfo()
    {
        memset( &info_, 0, sizeof(info_) );
        info_.cbSizeOf = sizeof(info_);
        // clear all buffers content
        WCHAR* field_buffers[] = { album, artist, date, filename, genre, title };
        BOOST_FOREACH(WCHAR* field_buffer, field_buffers) {
            memset( field_buffer, 0, kFIELDBUFFERSIZE * sizeof(field_buffer[0]) );
        }
        // set buffers length
        info_.nAlbumLen = info_.nArtistLen = info_.nDateLen
        = info_.nDateLen = info_.nFileNameLen = info_.nGenreLen = info_.nTitleLen
        = kFIELDBUFFERSIZEINCHARS;
        // set buffers
        info_.sAlbum = album;
        info_.sArtist = artist;
        info_.sDate = date;
        info_.sFileName = filename;
        info_.sGenre = genre;
        info_.sTitle = title;
    }

    void fixStringLengths()
    {
        // fill string lengths if Aimp do not do this.
        if (info_.nAlbumLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nAlbumLen = std::wcslen(info_.sAlbum);
        }

        if (info_.nArtistLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nArtistLen = std::wcslen(info_.sArtist);
        }

        if (info_.nDateLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nDateLen = std::wcslen(info_.sDate);
        }

        if (info_.nFileNameLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nFileNameLen = std::wcslen(info_.sFileName);
        }

        if (info_.nGenreLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nGenreLen = std::wcslen(info_.sGenre);
        }

        if (info_.nTitleLen == kFIELDBUFFERSIZEINCHARS) {
            info_.nTitleLen = std::wcslen(info_.sTitle);
        }
    }

    // assumption: fixEmptyTitle() method has been already called.
    void fixEmptyTitle()
    {
        if (info_.nTitleLen == 0) {
            boost::filesystem::wpath path(info_.sFileName);
            path.replace_extension();
            const boost::filesystem::wpath& filename = path.filename();
            info_.nTitleLen = std::min((size_t)kFIELDBUFFERSIZE, std::wcslen(filename.c_str()));
#pragma warning (push, 4)
#pragma warning( disable : 4996 )
            std::wcsncpy(info_.sTitle, filename.c_str(), info_.nTitleLen);
#pragma warning (pop)
        }
    }

    AIMP2SDK::AIMP2FileInfo info_;
    static const DWORD kFIELDBUFFERSIZE = MAX_PATH;
    static const DWORD kFIELDBUFFERSIZEINCHARS = kFIELDBUFFERSIZE - 1;
    WCHAR album[kFIELDBUFFERSIZE];
    WCHAR artist[kFIELDBUFFERSIZE];
    WCHAR date[kFIELDBUFFERSIZE];
    WCHAR filename[kFIELDBUFFERSIZE];
    WCHAR genre[kFIELDBUFFERSIZE];
    WCHAR title[kFIELDBUFFERSIZE];
};

void AIMPManager26::loadEntries(PlaylistID playlist_id) // throws std::runtime_error
{
    { // handle crc32.
        try {
            getPlaylistCRC32Object(playlist_id).reset_entries();
        } catch (std::exception& e) {
            throw std::runtime_error(MakeString() << "expected crc32 struct for playlist " << playlist_id << " not found in "__FUNCTION__". Reason: " << e.what());
        }
    }

    // PROFILE_EXECUTION_TIME(__FUNCTION__);
    const int entries_count = aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);

    AIMP2FileInfoHelper file_info_helper; // used for get entries from AIMP conveniently.
    
    deletePlaylistEntriesFromPlaylistDB(playlist_id); // remove old entries before adding new ones.

    sqlite3_stmt* stmt = createStmt(playlists_db_, "INSERT INTO PlaylistsEntries VALUES (?,?,?,?,?,?,"
                                                                                         "?,?,?,?,?,"
                                                                                         "?,?,?,?,?)"
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    //BOOST_LOG_SEV(logger(), debug) << "The statement has "
    //                               << sqlite3_bind_parameter_count(stmt)
    //                               << " wildcards";

#define bind(type, field_index, value)  rc_db = sqlite3_bind_##type(stmt, field_index, value); \
                                        if (SQLITE_OK != rc_db) { \
                                            const std::string msg = MakeString() << "Error sqlite3_bind_"#type << " " << rc_db; \
                                            throw std::runtime_error(msg); \
                                        }
#define bindText(field_index, info_field_name)  rc_db = sqlite3_bind_text16(stmt, field_index, info.s##info_field_name, info.n##info_field_name##Len * sizeof(WCHAR), SQLITE_STATIC); \
                                                if (SQLITE_OK != rc_db) { \
                                                    const std::string msg = MakeString() << "sqlite3_bind_text16" << " " << rc_db; \
                                                    throw std::runtime_error(msg); \
                                                }
    int rc_db;
    bind(int, 1, playlist_id);

    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        // aimp2_playlist_manager_->AIMP_PLS_Entry_ReloadInfo(id_, entry_index); // try to make AIMP update track info: this takes significant time and some tracks are not updated anyway.
        if ( aimp2_playlist_manager_->AIMP_PLS_Entry_InfoGet(playlist_id,
                                                             entry_index,
                                                             &file_info_helper.getEmptyFileInfo()
                                                             )
            )
        {
            // special db code
            {
                // bind all values
                const AIMP2SDK::AIMP2FileInfo& info = file_info_helper.getFileInfoWithCorrectStringLengthsAndNonEmptyTitle();
                bind(int,    2, entry_index); // id is the index for AIMP2.
                bind(int,    3, entry_index); // for consistency with AIMP3.
                bindText(    4, Album);
                bindText(    5, Artist);
                bindText(    6, Date);
                bindText(    7, FileName);
                bindText(    8, Genre);
                bindText(    9, Title);
                bind(int,   10, info.nBitRate);
                bind(int,   11, info.nChannels);
                bind(int,   12, info.nDuration);
                bind(int64, 13, info.nFileSize);
                bind(int,   14, info.nRating);
                bind(int,   15, info.nSampleRate);
                bind(int64, 16, crc32(info));

                rc_db = sqlite3_step(stmt);
                if (SQLITE_DONE != rc_db) {
                    const std::string msg = MakeString() << "sqlite3_step() error "
                                                         << rc_db << ": " << sqlite3_errmsg(playlists_db_);
                    throw std::runtime_error(msg);
                }
                sqlite3_reset(stmt);
            }
        } else {
            BOOST_LOG_SEV(logger(), error) << "Error occured while getting entry info ¹" << entry_index << " from playlist with ID = " << playlist_id;
            throw std::runtime_error("Error occured while getting playlist entries.");
        }
    }
#undef bind
#undef bindText
}

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

bool playlistExists(PlaylistID playlist_id, boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp2_playlist_manager) 
{
    const size_t playlistsCount = aimp2_playlist_manager->AIMP_PLS_Count();
    for (size_t playlist_index = 0; playlist_index < playlistsCount; ++playlist_index) {
        int id;
        int result = aimp2_playlist_manager->AIMP_PLS_ID_By_Index(playlist_index, &id);
        assert(S_OK == result); (void)result;
        if (id == playlist_id) {
            return true;
        }
    }
    return false;
}

bool playlistExists(PlaylistID playlist_id, sqlite3* db, int* entries_count)
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT entries_count FROM Playlists WHERE id=" << playlist_id;

    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            if (entries_count) {
                *entries_count = sqlite3_column_int(stmt, 0);
            }
            return true;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }
    return false;
}

int playlistsCount(sqlite3* db)
{
    using namespace Utilities;

    std::ostringstream query;
    
    query << "SELECT COUNT(*) FROM Playlists";

    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            return sqlite3_column_int(stmt, 0);
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }
    assert(!"Unexpected query result in "__FUNCTION__);
    return 0;
}

void foreach_row(const std::string& query, sqlite3* db, std::function<void(sqlite3_stmt*)> row_callback)
{
    using namespace Utilities;

    sqlite3_stmt* stmt = createStmt(db, query);
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            row_callback(stmt);
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query;
            throw std::runtime_error(msg);
		}
    }
}

void AIMPManager26::checkIfPlaylistsChanged()
{
    const int playlist_count = playlistsCount(playlists_db_);

    std::vector<PlaylistID> playlists_to_reload;
    const size_t current_playlists_count = aimp2_playlist_manager_->AIMP_PLS_Count();
    for (size_t playlist_index = 0; playlist_index < current_playlists_count; ++playlist_index) {
        int current_playlist_id;
        if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_ID_By_Index(playlist_index, &current_playlist_id) ) {
            throw std::runtime_error(MakeString() << "checkIfPlaylistsChanged() failed. Reason: AIMP_PLS_ID_By_Index failed");
        }

        const int current_entries_count = aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(current_playlist_id);

        int entries_count = 0;
        if (!playlistExists(current_playlist_id, playlists_db_, &entries_count)) {
            // current playlist was not loaded yet, load it now without entries.
            loadPlaylist(playlist_index);
            playlists_to_reload.push_back(current_playlist_id);
        } else if (entries_count != current_entries_count) {
            // list loaded, but entries count is changed: load new playlist to update all internal fields.
            loadPlaylist(playlist_index);
            playlists_to_reload.push_back(current_playlist_id);
        } else if ( !isLoadedPlaylistEqualsAimpPlaylist(current_playlist_id) ) {
            // contents of loaded playlist and current playlist are different.
            playlists_to_reload.push_back(current_playlist_id);
        }
    }

    // reload entries of playlists from list.
    for (PlaylistID playlist_id : playlists_to_reload) {
        try {
            loadEntries(playlist_id); // avoid using of playlists_[playlist_id], since it requires Playlist's default ctor.
            updatePlaylistCrcInDB( playlist_id, getPlaylistCRC32(playlist_id) );
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error occured while playlist(id = " << playlist_id << ") entries loading. Reason: " << e.what();
        }
    }

    bool playlists_content_changed = !playlists_to_reload.empty();

    // handle closed playlists.
    if (current_playlists_count < static_cast<size_t>(playlist_count)) { 
        playlists_content_changed = true;

        std::vector<PlaylistID> playlists_to_delete;

        auto handler = [&playlists_to_delete, this](sqlite3_stmt* stmt) {
            const PlaylistID id = sqlite3_column_int(stmt, 0);
            if (!playlistExists(id, aimp2_playlist_manager_) ) {
                playlists_to_delete.push_back(id);
            }
        };
        foreach_row("SELECT id FROM Playlists", playlists_db_, handler);

        for (PlaylistID playlist_id : playlists_to_delete) {
            playlist_crc32_list_.erase(playlist_id);
            deletePlaylistFromPlaylistDB(playlist_id);
        }
    }
 
    if (playlists_content_changed) {
        notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
    }
}

void AIMPManager26::updatePlaylistCrcInDB(PlaylistID playlist_id, crc32_t crc32)
{
    sqlite3_stmt* stmt = createStmt(playlists_db_,
                                    "UPDATE Playlists SET crc32=? WHERE id=?"
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

#define bind(type, field_index, value)  rc_db = sqlite3_bind_##type(stmt, field_index, value); \
                                        if (SQLITE_OK != rc_db) { \
                                            const std::string msg = MakeString() << "Error sqlite3_bind_"#type << " " << rc_db; \
                                            throw std::runtime_error(msg); \
                                        }

    int rc_db;
    bind(int64, 1, crc32);
    bind(int,   2, playlist_id );
    
#undef bind
    rc_db = sqlite3_step(stmt);
    if (SQLITE_DONE != rc_db) {
        const std::string msg = MakeString() << "sqlite3_step() error "
                                             << rc_db << ": " << sqlite3_errmsg(playlists_db_);
        throw std::runtime_error(msg);
    }
}

namespace {

crc32_t crc32_text16(const void* text16) 
{
    const wchar_t* text = static_cast<const wchar_t*>(text16);
    return Utilities::crc32( text, wcslen(text) * sizeof(text[0]) );
}

crc32_t crc32_entry(sqlite3_stmt* stmt)
{
    assert(sqlite3_column_count(stmt) == 12);

    using namespace Utilities;
    const crc32_t members_crc32_list [] = {
            crc32_text16( sqlite3_column_text16(stmt, 0) ),
            crc32_text16( sqlite3_column_text16(stmt, 1) ),
            crc32_text16( sqlite3_column_text16(stmt, 2) ),
            crc32_text16( sqlite3_column_text16(stmt, 3) ),
            crc32_text16( sqlite3_column_text16(stmt, 4) ),
            crc32_text16( sqlite3_column_text16(stmt, 5) ),
                          sqlite3_column_int   (stmt, 6),
                          sqlite3_column_int   (stmt, 7),
                          sqlite3_column_int   (stmt, 8),
        Utilities::crc32( sqlite3_column_int64 (stmt, 9) ),
                          sqlite3_column_int   (stmt,10),
                          sqlite3_column_int   (stmt,11),
    };

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}

} //namespace

bool AIMPManager26::isLoadedPlaylistEqualsAimpPlaylist(PlaylistID playlist_id) const
{
    // PROFILE_EXECUTION_TIME(__FUNCTION__);

    const int entries_count = aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);

#if _DEBUG
    const size_t loaded_entries_count = getEntriesCountDB(playlist_id, playlists_db_);
    assert(entries_count >= 0 && static_cast<size_t>(entries_count) == loaded_entries_count); // function returns correct result only if entries count in loaded and actual playlists are equal.
#endif

    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT "
          << "album, artist, date, filename, genre, title, bitrate, channels_count, duration, filesize, rating, samplerate"
          << " FROM PlaylistsEntries WHERE playlist_id=" << playlist_id << " ORDER BY entry_index";

    sqlite3* db = playlists_db_;
    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    AIMP2FileInfoHelper file_info_helper; // used for get entries from AIMP conveniently.
    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        if ( !aimp2_playlist_manager_->AIMP_PLS_Entry_InfoGet( playlist_id,
                                                               entry_index,
                                                               &file_info_helper.getEmptyFileInfo()
                                                               )
            )
        {
            // in case of Aimp error just say that playlists are not equal.
            return false;
        }

		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            // need to compare loaded_entry with file_info_helper.info_;
            const AIMP2SDK::AIMP2FileInfo& aimp_entry = file_info_helper.getFileInfoWithCorrectStringLengthsAndNonEmptyTitle();
            if ( crc32_entry(stmt) != Utilities::crc32(aimp_entry) ) {
                return false;
            }
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                    << rc_db << ": " << sqlite3_errmsg(db)
                                                    << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    return true;
}

void AIMPManager26::onTimerPlaylistsChangeCheck(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        // Restart timer.
        playlists_check_timer_.expires_from_now( boost::posix_time::seconds(kPLAYLISTS_CHECK_PERIOD_SEC) );
        playlists_check_timer_.async_wait( boost::bind( &AIMPManager26::onTimerPlaylistsChangeCheck, this, _1 ) );

        // Do check
        checkIfPlaylistsChanged();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

#endif // #ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

void AIMPManager26::registerNotifiers()
{
    using namespace boost::assign;
    insert(aimp_callback_names_)
        (AIMP_STATUS_CHANGE,     "AIMP_STATUS_CHANGE")
        (AIMP_PLAY_FILE,         "AIMP_PLAY_FILE")
        (AIMP_INFO_UPDATE,       "AIMP_INFO_UPDATE")
        (AIMP_PLAYER_STATE,      "AIMP_PLAYER_STATE")
        (AIMP_EFFECT_CHANGED,    "AIMP_EFFECT_CHANGED")
        (AIMP_EQ_CHANGED,        "AIMP_EQ_CHANGED")
        (AIMP_TRACK_POS_CHANGED, "AIMP_TRACK_POS_CHANGED")
    ;

    BOOST_FOREACH(const auto& callback, aimp_callback_names_) {
        const int callback_id = callback.first;
        // register notifier.
        boolean result = aimp2_controller_->AIMP_CallBack_Set( callback_id,
                                                               &internalAIMPStateNotifier,
                                                               reinterpret_cast<DWORD>(this) // user data that will be passed in internalAIMPStateNotifier().
                                                              );
        if (!result) {
            BOOST_LOG_SEV(logger(), error) << "Error occured while register " << callback.second << " callback";
        }
    }
}

void AIMPManager26::unregisterNotifiers()
{
    BOOST_FOREACH(const auto& callback, aimp_callback_names_) {
        const int callback_id = callback.first;
        // unregister notifier.
        const boolean result = aimp2_controller_->AIMP_CallBack_Remove(callback_id, &internalAIMPStateNotifier);
        if (!result) {
            BOOST_LOG_SEV(logger(), error) <<  "Error occured while unregister " << callback.second << " callback";
        }
    }
}

void AIMPManager26::initializeAIMPObjects()
{
    // get IAIMP2Player object.
    {
    IAIMP2Player* player = nullptr;
    boolean result = aimp2_controller_->AIMP_QueryObject(IAIMP2PlayerID, &player);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2Player failed");
    }
    aimp2_player_.reset(player);
    player->Release();
    }

    // get IAIMP2PlaylistManager2 object.
    {
    IAIMP2PlaylistManager2* playlist_manager = nullptr;
    boolean result = aimp2_controller_->AIMP_QueryObject(IAIMP2PlaylistManager2ID, &playlist_manager);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2PlaylistManager2 failed");
    }
    aimp2_playlist_manager_.reset(playlist_manager);
    playlist_manager->Release();
    }

    // get IAIMP2Extended object.
    {
    IAIMP2Extended* extended = nullptr;
    boolean result = aimp2_controller_->AIMP_QueryObject(IAIMP2ExtendedID, &extended);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2Extended failed");
    }
    aimp2_extended_.reset(extended);
    extended->Release();
    }

    // get IAIMP2CoverArtManager object.
    {
    IAIMP2CoverArtManager* cover_art_manager = nullptr;
    boolean result = aimp2_controller_->AIMP_QueryObject(IAIMP2CoverArtManagerID, &cover_art_manager);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2CoverArtManager failed");
    }
    aimp2_cover_art_manager_.reset(cover_art_manager);
    cover_art_manager->Release();
    }
}

void AIMPManager26::startPlayback()
{
    // play current track.
    aimp2_player_->PlayOrResume();
}

void AIMPManager26::startPlayback(TrackDescription track_desc) // throws std::runtime_error
{
    track_desc = getAbsoluteTrackDesc(track_desc);
    if ( FALSE == aimp2_player_->PlayTrack(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__" with " << track_desc );
    }
}

void AIMPManager26::stopPlayback()
{
    aimp2_player_->Stop();
}

void AIMPManager26::pausePlayback()
{
    aimp2_player_->Pause();
}

void AIMPManager26::playNextTrack()
{
    aimp2_player_->NextTrack();
}

void AIMPManager26::playPreviousTrack()
{
    aimp2_player_->PrevTrack();
}


//template<typename To, typename From> To cast(From);
//typedef int AIMP2SDK_STATUS;

template<>
AIMP2SDK_STATUS cast(AIMPManager::STATUS status) // throws std::bad_cast
{
    using namespace AIMP2SDK;
    switch (status) {
    case AIMPManager::STATUS_VOLUME:    return AIMP_STS_VOLUME;
    case AIMPManager::STATUS_BALANCE:   return AIMP_STS_BALANCE;
    case AIMPManager::STATUS_SPEED:     return AIMP_STS_SPEED;
    case AIMPManager::STATUS_Player:    return AIMP_STS_Player;
    case AIMPManager::STATUS_MUTE:      return AIMP_STS_MUTE;
    case AIMPManager::STATUS_REVERB:    return AIMP_STS_REVERB;
    case AIMPManager::STATUS_ECHO:      return AIMP_STS_ECHO;
    case AIMPManager::STATUS_CHORUS:    return AIMP_STS_CHORUS;
    case AIMPManager::STATUS_Flanger:   return AIMP_STS_Flanger;

    case AIMPManager::STATUS_EQ_STS:    return AIMP_STS_EQ_STS;
    case AIMPManager::STATUS_EQ_SLDR01: return AIMP_STS_EQ_SLDR01;
    case AIMPManager::STATUS_EQ_SLDR02: return AIMP_STS_EQ_SLDR02;
    case AIMPManager::STATUS_EQ_SLDR03: return AIMP_STS_EQ_SLDR03;
    case AIMPManager::STATUS_EQ_SLDR04: return AIMP_STS_EQ_SLDR04;
    case AIMPManager::STATUS_EQ_SLDR05: return AIMP_STS_EQ_SLDR05;
    case AIMPManager::STATUS_EQ_SLDR06: return AIMP_STS_EQ_SLDR06;
    case AIMPManager::STATUS_EQ_SLDR07: return AIMP_STS_EQ_SLDR07;
    case AIMPManager::STATUS_EQ_SLDR08: return AIMP_STS_EQ_SLDR08;
    case AIMPManager::STATUS_EQ_SLDR09: return AIMP_STS_EQ_SLDR09;
    case AIMPManager::STATUS_EQ_SLDR10: return AIMP_STS_EQ_SLDR10;
    case AIMPManager::STATUS_EQ_SLDR11: return AIMP_STS_EQ_SLDR11;
    case AIMPManager::STATUS_EQ_SLDR12: return AIMP_STS_EQ_SLDR12;
    case AIMPManager::STATUS_EQ_SLDR13: return AIMP_STS_EQ_SLDR13;
    case AIMPManager::STATUS_EQ_SLDR14: return AIMP_STS_EQ_SLDR14;
    case AIMPManager::STATUS_EQ_SLDR15: return AIMP_STS_EQ_SLDR15;
    case AIMPManager::STATUS_EQ_SLDR16: return AIMP_STS_EQ_SLDR16;
    case AIMPManager::STATUS_EQ_SLDR17: return AIMP_STS_EQ_SLDR17;
    case AIMPManager::STATUS_EQ_SLDR18: return AIMP_STS_EQ_SLDR18;

    case AIMPManager::STATUS_REPEAT:    return AIMP_STS_REPEAT;
    case AIMPManager::STATUS_STAY_ON_TOP: return AIMP_STS_ON_STOP;
    case AIMPManager::STATUS_POS:       return AIMP_STS_POS;
    case AIMPManager::STATUS_LENGTH:    return AIMP_STS_LENGTH;
    case AIMPManager::STATUS_ACTION_ON_END_OF_PLAYLIST: return AIMP_STS_REPEATPLS;
    case AIMPManager::STATUS_REPEAT_SINGLE_FILE_PLAYLISTS: return AIMP_STS_REP_PLS_1;
    case AIMPManager::STATUS_KBPS:      return AIMP_STS_KBPS;
    case AIMPManager::STATUS_KHZ:       return AIMP_STS_KHZ;
    case AIMPManager::STATUS_MODE:      return AIMP_STS_MODE;
    case AIMPManager::STATUS_RADIO_CAPTURE: return AIMP_STS_RADIO;
    case AIMPManager::STATUS_STREAM_TYPE: return AIMP_STS_STREAM_TYPE;
    case AIMPManager::STATUS_REVERSETIME: return AIMP_STS_TIMER;
    case AIMPManager::STATUS_SHUFFLE:   return AIMP_STS_SHUFFLE;
    case AIMPManager::STATUS_MAIN_HWND: return AIMP_STS_MAIN_HWND;
    case AIMPManager::STATUS_TC_HWND:   return AIMP_STS_TC_HWND;
    case AIMPManager::STATUS_APP_HWND:  return AIMP_STS_APP_HWND;
    case AIMPManager::STATUS_PL_HWND:   return AIMP_STS_PL_HWND;
    case AIMPManager::STATUS_EQ_HWND:   return AIMP_STS_EQ_HWND;
    case AIMPManager::STATUS_TRAY:      return AIMP_STS_TRAY;
    default:
        break;
    }

    assert(!"unknown AIMPSDK status in "__FUNCTION__);
    throw std::bad_cast( std::string(MakeString() << "can't cast AIMPManager::STATUS status " << static_cast<int>(status) << " to AIMPSDK status"
                                     ).c_str()
                        );
}

template<>
AIMPManager26::STATUS cast(AIMP2SDK_STATUS status) // throws std::bad_cast
{
    using namespace AIMP2SDK;
    switch (status) {
    case AIMP_STS_VOLUME:    return AIMPManager::STATUS_VOLUME;
    case AIMP_STS_BALANCE:   return AIMPManager::STATUS_BALANCE;
    case AIMP_STS_SPEED:     return AIMPManager::STATUS_SPEED;
    case AIMP_STS_Player:    return AIMPManager::STATUS_Player;
    case AIMP_STS_MUTE:      return AIMPManager::STATUS_MUTE;
    case AIMP_STS_REVERB:    return AIMPManager::STATUS_REVERB;
    case AIMP_STS_ECHO:      return AIMPManager::STATUS_ECHO;
    case AIMP_STS_CHORUS:    return AIMPManager::STATUS_CHORUS;
    case AIMP_STS_Flanger:   return AIMPManager::STATUS_Flanger;

    case AIMP_STS_EQ_STS:    return AIMPManager::STATUS_EQ_STS;
    case AIMP_STS_EQ_SLDR01: return AIMPManager::STATUS_EQ_SLDR01;
    case AIMP_STS_EQ_SLDR02: return AIMPManager::STATUS_EQ_SLDR02;
    case AIMP_STS_EQ_SLDR03: return AIMPManager::STATUS_EQ_SLDR03;
    case AIMP_STS_EQ_SLDR04: return AIMPManager::STATUS_EQ_SLDR04;
    case AIMP_STS_EQ_SLDR05: return AIMPManager::STATUS_EQ_SLDR05;
    case AIMP_STS_EQ_SLDR06: return AIMPManager::STATUS_EQ_SLDR06;
    case AIMP_STS_EQ_SLDR07: return AIMPManager::STATUS_EQ_SLDR07;
    case AIMP_STS_EQ_SLDR08: return AIMPManager::STATUS_EQ_SLDR08;
    case AIMP_STS_EQ_SLDR09: return AIMPManager::STATUS_EQ_SLDR09;
    case AIMP_STS_EQ_SLDR10: return AIMPManager::STATUS_EQ_SLDR10;
    case AIMP_STS_EQ_SLDR11: return AIMPManager::STATUS_EQ_SLDR11;
    case AIMP_STS_EQ_SLDR12: return AIMPManager::STATUS_EQ_SLDR12;
    case AIMP_STS_EQ_SLDR13: return AIMPManager::STATUS_EQ_SLDR13;
    case AIMP_STS_EQ_SLDR14: return AIMPManager::STATUS_EQ_SLDR14;
    case AIMP_STS_EQ_SLDR15: return AIMPManager::STATUS_EQ_SLDR15;
    case AIMP_STS_EQ_SLDR16: return AIMPManager::STATUS_EQ_SLDR16;
    case AIMP_STS_EQ_SLDR17: return AIMPManager::STATUS_EQ_SLDR17;
    case AIMP_STS_EQ_SLDR18: return AIMPManager::STATUS_EQ_SLDR18;

    case AIMP_STS_REPEAT:    return AIMPManager::STATUS_REPEAT;
    case AIMP_STS_ON_STOP:   return AIMPManager::STATUS_STAY_ON_TOP;
    case AIMP_STS_POS:       return AIMPManager::STATUS_POS;
    case AIMP_STS_LENGTH:    return AIMPManager::STATUS_LENGTH;
    case AIMP_STS_REPEATPLS: return AIMPManager::STATUS_ACTION_ON_END_OF_PLAYLIST;
    case AIMP_STS_REP_PLS_1: return AIMPManager::STATUS_REPEAT_SINGLE_FILE_PLAYLISTS;
    case AIMP_STS_KBPS:      return AIMPManager::STATUS_KBPS;
    case AIMP_STS_KHZ:       return AIMPManager::STATUS_KHZ;
    case AIMP_STS_MODE:      return AIMPManager::STATUS_MODE;
    case AIMP_STS_RADIO:     return AIMPManager::STATUS_RADIO_CAPTURE;
    case AIMP_STS_STREAM_TYPE: return AIMPManager::STATUS_STREAM_TYPE;
    case AIMP_STS_TIMER:     return AIMPManager::STATUS_REVERSETIME;
    case AIMP_STS_SHUFFLE:   return AIMPManager::STATUS_SHUFFLE;
    case AIMP_STS_MAIN_HWND: return AIMPManager::STATUS_MAIN_HWND;
    case AIMP_STS_TC_HWND:   return AIMPManager::STATUS_TC_HWND;
    case AIMP_STS_APP_HWND:  return AIMPManager::STATUS_APP_HWND;
    case AIMP_STS_PL_HWND:   return AIMPManager::STATUS_PL_HWND;
    case AIMP_STS_EQ_HWND:   return AIMPManager::STATUS_EQ_HWND;
    case AIMP_STS_TRAY:      return AIMPManager::STATUS_TRAY;
    default:
        break;
    }

    assert(!"unknown AIMPSDK status in "__FUNCTION__);
    throw std::bad_cast( std::string(MakeString() << "can't cast AIMP SDK status " << status << " to AIMPManager::STATUS"
                                     ).c_str()
                        );
}

const char* asString(AIMP2SDK_STATUS status)
{
    using namespace AIMP2SDK;
    switch (status) {
    case AIMP_STS_VOLUME:    return "VOLUME";
    case AIMP_STS_BALANCE:   return "BALANCE";
    case AIMP_STS_SPEED:     return "SPEED";
    case AIMP_STS_Player:    return "Player";
    case AIMP_STS_MUTE:      return "MUTE";
    case AIMP_STS_REVERB:    return "REVERB";
    case AIMP_STS_ECHO:      return "ECHO";
    case AIMP_STS_CHORUS:    return "CHORUS";
    case AIMP_STS_Flanger:   return "Flanger";

    case AIMP_STS_EQ_STS:    return "EQ_STS";
    case AIMP_STS_EQ_SLDR01: return "EQ_SLDR01";
    case AIMP_STS_EQ_SLDR02: return "EQ_SLDR02";
    case AIMP_STS_EQ_SLDR03: return "EQ_SLDR03";
    case AIMP_STS_EQ_SLDR04: return "EQ_SLDR04";
    case AIMP_STS_EQ_SLDR05: return "EQ_SLDR05";
    case AIMP_STS_EQ_SLDR06: return "EQ_SLDR06";
    case AIMP_STS_EQ_SLDR07: return "EQ_SLDR07";
    case AIMP_STS_EQ_SLDR08: return "EQ_SLDR08";
    case AIMP_STS_EQ_SLDR09: return "EQ_SLDR09";
    case AIMP_STS_EQ_SLDR10: return "EQ_SLDR10";
    case AIMP_STS_EQ_SLDR11: return "EQ_SLDR11";
    case AIMP_STS_EQ_SLDR12: return "EQ_SLDR12";
    case AIMP_STS_EQ_SLDR13: return "EQ_SLDR13";
    case AIMP_STS_EQ_SLDR14: return "EQ_SLDR14";
    case AIMP_STS_EQ_SLDR15: return "EQ_SLDR15";
    case AIMP_STS_EQ_SLDR16: return "EQ_SLDR16";
    case AIMP_STS_EQ_SLDR17: return "EQ_SLDR17";
    case AIMP_STS_EQ_SLDR18: return "EQ_SLDR18";

    case AIMP_STS_REPEAT:    return "REPEAT";
    case AIMP_STS_ON_STOP:   return "ON_STOP";
    case AIMP_STS_POS:       return "POS";
    case AIMP_STS_LENGTH:    return "LENGTH";
    case AIMP_STS_REPEATPLS: return "REPEATPLS";
    case AIMP_STS_REP_PLS_1: return "REP_PLS_1";
    case AIMP_STS_KBPS:      return "KBPS";
    case AIMP_STS_KHZ:       return "KHZ";
    case AIMP_STS_MODE:      return "MODE";
    case AIMP_STS_RADIO:     return "RADIO";
    case AIMP_STS_STREAM_TYPE: return "STREAM_TYPE";
    case AIMP_STS_TIMER:     return "TIMER";
    case AIMP_STS_SHUFFLE:   return "SHUFFLE";
    case AIMP_STS_MAIN_HWND: return "MAIN_HWND";
    case AIMP_STS_TC_HWND:   return "TC_HWND";
    case AIMP_STS_APP_HWND:  return "APP_HWND";
    case AIMP_STS_PL_HWND:   return "PL_HWND";
    case AIMP_STS_EQ_HWND:   return "EQ_HWND";
    case AIMP_STS_TRAY:      return "TRAY";
    default:
        break;
    }

    assert(!"unknown AIMPSDK status in "__FUNCTION__);
    throw std::bad_cast( std::string(MakeString() << "can't find string representation of AIMP SDK status " << status).c_str()
                        );
}

bool AIMPManager26::getEventRelatedTo(AIMPManager26::STATUS status, AIMPManager26::EVENTS* event)
{
    assert(event);

    switch (status) {
    case STATUS_SHUFFLE:
        *event = EVENT_SHUFFLE;
        break;
    case STATUS_REPEAT:
        *event = EVENT_REPEAT;
        break;
    case STATUS_VOLUME:
        *event = EVENT_VOLUME;
        break;
    case STATUS_MUTE:
        *event = EVENT_MUTE;
        break;
    case STATUS_POS:
        *event = EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY;
        break;
    case STATUS_RADIO_CAPTURE:
        *event = EVENT_RADIO_CAPTURE;
        break;
    default:
        return false;
    }

    return true;
}

AIMPManager26::StatusValue convertValue(AIMPManager26::STATUS status, const AIMPManager26::StatusValue value)
{
    AIMPManager26::StatusValue new_value = value;
    // convert some values
    switch (status) {
    case AIMPManager26::STATUS_REPEAT_SINGLE_FILE_PLAYLISTS:
        new_value = !value;
    default:
        break; 
    }
    return new_value;
}

void AIMPManager26::setStatus(AIMPManager26::STATUS status, AIMPManager26::StatusValue value)
{
    try {
        if ( FALSE == aimp2_controller_->AIMP_Status_Set(cast<AIMP2SDK_STATUS>(status), 
                                                         convertValue(status, value) 
                                                         ) 
           )
        {
            throw std::runtime_error(MakeString() << "Error occured while setting status " << asString(status) << " to value " << value);
        }
    } catch (std::bad_cast& e) {
        throw std::runtime_error( e.what() );
    }

    EVENTS event;
    if (getEventRelatedTo(status, &event)) { 
        notifyAllExternalListeners(event);
    }
}

AIMPManager26::StatusValue AIMPManager26::getStatus(AIMPManager26::STATUS status) const
{
    AIMPManager26::StatusValue value = convertValue(status, 
                                                   aimp2_controller_->AIMP_Status_Get(status) 
                                                   );
    return value;
}

void AIMPManager26::onTick()
{
    struct StatusDesc { int key, value; };
    static bool inited = false;
    const int status_count = STATUS_LAST - STATUS_FIRST - 1;
    static StatusDesc status_desc[status_count];
    if (!inited) {
        inited = true;
        for(int i = 0; i != status_count; ++i) {
            StatusDesc& desc = status_desc[i];
            desc.key = STATUS_FIRST + i + 1;
            desc.value = aimp2_controller_->AIMP_Status_Get(desc.key);
        }
    }

    for(int i = 0; i != status_count; ++i) {
        StatusDesc& desc = status_desc[i];
        const int status = desc.key;
        const int new_value = aimp2_controller_->AIMP_Status_Get(status);
        if (desc.value != new_value) {
            if (STATUS_POS != desc.key) {
                BOOST_LOG_SEV(logger(), debug) << "status(STATUS_" << asString(status) << ") changed from " << desc.value << " to " << new_value;
            }
            desc.value = new_value;
        }
    }
}

std::string AIMPManager26::getAIMPVersion() const
{
    const int version = aimp2_controller_->AIMP_GetSystemVersion(); // IAIMP2Player::Version() is not used since it is always returns 1. Maybe it is Aimp SDK version?
    using namespace std;
    ostringstream os;
    os << version / 1000 << '.' << setfill('0') << setw(2) << (version % 1000) / 10 << '.' << version % 10;
    return os.str();
}

PlaylistID AIMPManager26::getAbsolutePlaylistID(PlaylistID id) const
{
    if (id == -1) { // treat ID -1 as playing playlist.
        id = getPlayingPlaylist();
    }
    return id;
}

PlaylistEntryID AIMPManager26::getAbsoluteEntryID(PlaylistEntryID id) const // throws std::runtime_error
{
    if (id == -1) { // treat ID -1 as playing entry.
        id = getPlayingEntry();
    }

    return id;
}

TrackDescription AIMPManager26::getAbsoluteTrackDesc(TrackDescription track_desc) const // throws std::runtime_error
{
    return TrackDescription( getAbsolutePlaylistID(track_desc.playlist_id), getAbsoluteEntryID(track_desc.track_id) );
}

PlaylistID AIMPManager26::getPlayingPlaylist() const
{
    // return AIMP internal playlist ID here since AIMPManager26 uses the same IDs.
    return aimp2_playlist_manager_->AIMP_PLS_ID_PlayingGet();
}

PlaylistEntryID AIMPManager26::getPlayingEntry() const
{
    const PlaylistID active_playlist = getPlayingPlaylist();
    const int internal_active_entry_index = aimp2_playlist_manager_->AIMP_PLS_ID_PlayingGetTrackIndex(active_playlist);
    // internal index equals AIMPManager26 ID. In other case map index<->ID(use Playlist::entries_id_list_) here in all places where TrackDescription is used.
    const PlaylistEntryID entry_id = internal_active_entry_index;
    return entry_id;
}

TrackDescription AIMPManager26::getPlayingTrack() const
{
    return TrackDescription( getPlayingPlaylist(), getPlayingEntry() );
}

AIMPManager::PLAYLIST_ENTRY_SOURCE_TYPE AIMPManager26::getTrackSourceType(TrackDescription track_desc) const // throws std::runtime_error
{
    const DWORD duration = getEntryField<DWORD>(playlists_db_, "duration", getAbsoluteTrackDesc(track_desc));
    return duration == 0 ? SOURCE_TYPE_RADIO : SOURCE_TYPE_FILE; // very shallow determination. Duration can be 0 on usual track if AIMP has no loaded track info yet.
}

AIMPManager::PLAYBACK_STATE AIMPManager26::getPlaybackState() const
{
    PLAYBACK_STATE state = STOPPED;
    StatusValue internal_state = getStatus(STATUS_Player);
    // map internal AIMP state to PLAYBACK_STATE.
    switch (internal_state) {
    case 0:
        state = STOPPED;
        break;
    case 1:
        state = PLAYING;
        break;
    case 2:
        state = PAUSED;
        break;
    default:
        assert(!"getStatus(STATUS_Player) returned unknown value");
        BOOST_LOG_SEV(logger(), error) << "getStatus(STATUS_Player) returned unknown value " << internal_state;
    }

    return state;
}

void AIMPManager26::enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) // throws std::runtime_error
{
    track_desc = getAbsoluteTrackDesc(track_desc);
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_Entry_QueueSet(track_desc.playlist_id, track_desc.track_id, insert_at_queue_beginning) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager26::removeEntryFromPlayQueue(TrackDescription track_desc) // throws std::runtime_error
{
    track_desc = getAbsoluteTrackDesc(track_desc);
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_Entry_QueueRemove(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

int AIMPManager26::trackRating(TrackDescription track_desc) const // throws std::runtime_error
{
    return getEntryField<DWORD>(playlists_db_, "rating", getAbsoluteTrackDesc(track_desc));
}

void AIMPManager26::saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width, int cover_height) const // throw std::runtime_error
{
    track_desc = getAbsoluteTrackDesc(track_desc);

    try {
        using namespace ImageUtils;
        std::auto_ptr<AIMPCoverImage> cover( getCoverImage(track_desc, cover_width, cover_height) );
        cover->saveToFile(filename);
    } catch (std::exception& e) {
        const std::string& str = MakeString() << "Error occured while cover saving to file for " << track_desc << ". Reason: " << e.what();
        BOOST_LOG_SEV(logger(), error) << str;
        throw std::runtime_error(str);
    }
}

std::auto_ptr<ImageUtils::AIMPCoverImage> AIMPManager26::getCoverImage(TrackDescription track_desc, int cover_width, int cover_height) const
{
    if (cover_width < 0 || cover_height < 0) {
        throw std::invalid_argument(MakeString() << "Error in "__FUNCTION__ << ". Negative cover size.");
    }

    const std::wstring& entry_filename = getEntryField<std::wstring>(playlists_db_, "filename", track_desc);
    const SIZE request_full_size = { 0, 0 };
    HBITMAP cover_bitmap_handle = aimp2_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry_filename.c_str() ), &request_full_size);

    // get real bitmap size
    const SIZE cover_full_size = ImageUtils::getBitmapSize(cover_bitmap_handle);
    if (cover_full_size.cx != 0 && cover_full_size.cy != 0) {
        SIZE cover_size;
        if (cover_width != 0 && cover_height != 0) {
            // specified size
            cover_size.cx = cover_width;
            cover_size.cy = cover_height;
        } else if (cover_width == 0 && cover_height == 0) {
            // original size
            cover_size.cx = cover_full_size.cx;
            cover_size.cy = cover_full_size.cy;
        } else if (cover_height == 0) {
            // specified width, proportional height
            cover_size.cx = cover_width;
            cover_size.cy = LONG( float(cover_full_size.cy) * float(cover_width) / float(cover_full_size.cx) );
        } else if (cover_width == 0) {
            // specified height, proportional width
            cover_size.cx = LONG( float(cover_full_size.cx) * float(cover_height) / float(cover_full_size.cy) );
            cover_size.cy = cover_height;
        }

        cover_bitmap_handle = aimp2_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry_filename.c_str() ), &cover_size);
    }

    using namespace ImageUtils;
    return std::auto_ptr<AIMPCoverImage>( new AIMPCoverImage(cover_bitmap_handle) ); // do not close handle of AIMP bitmap.
}

void getEntryField_(sqlite3* db, const char* field, TrackDescription track_desc, std::function<void(sqlite3_stmt*)> row_callback)
{
    using namespace Utilities;

    std::ostringstream query;

    query << "SELECT " << field
          << " FROM PlaylistsEntries WHERE playlist_id=" << track_desc.playlist_id << " AND entry_id=" << track_desc.track_id;

    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);
    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            assert(sqlite3_column_count(stmt) == 1);
            row_callback(stmt);
            return;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    throw std::runtime_error(MakeString() << "Error in "__FUNCTION__ << ". Track " << track_desc << " does not exist");
}

template<>
std::wstring getEntryField(sqlite3* db, const char* field, TrackDescription track_desc)
{
    std::wstring r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_TEXT);
        if (sqlite3_column_type(stmt, 0) != SQLITE_TEXT) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Track " << track_desc);
        }
        r = static_cast<const std::wstring::value_type*>(sqlite3_column_text16(stmt, 0));
    };
    getEntryField_(db, field, track_desc, handler);
    return r;
}

template<>
DWORD getEntryField(sqlite3* db, const char* field, TrackDescription track_desc)
{
    DWORD r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
        if (sqlite3_column_type(stmt, 0) != SQLITE_INTEGER) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Track " << track_desc);
        }
        r = static_cast<DWORD>(sqlite3_column_int(stmt, 0));
    };
    getEntryField_(db, field, track_desc, handler);
    return r;
}

template<>
INT64 getEntryField(sqlite3* db, const char* field, TrackDescription track_desc)
{
    INT64 r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
        if (sqlite3_column_type(stmt, 0) != SQLITE_INTEGER) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Track " << track_desc);
        }
        r = sqlite3_column_int64(stmt, 0);
    };
    getEntryField_(db, field, track_desc, handler);
    return r;
}

void WINAPI AIMPManager26::internalAIMPStateNotifier(DWORD User, DWORD dwCBType)
{
    AIMPManager26* aimp_manager = reinterpret_cast<AIMPManager26*>(User);
    EVENTS event = EVENTS_COUNT;
    switch (dwCBType)
    {
    case AIMP_TRACK_POS_CHANGED: // it is sent with period 1 second.
        event = EVENT_TRACK_POS_CHANGED;
        break;
    case AIMP_PLAY_FILE: // sent when playback started.
        event = EVENT_PLAY_FILE;
        break;
    case AIMP_PLAYER_STATE:
        event = EVENT_PLAYER_STATE;
        break;
    case AIMP_STATUS_CHANGE:
        event = EVENT_STATUS_CHANGE;
        break;
    case AIMP_INFO_UPDATE:
        event = EVENT_INFO_UPDATE;
        break;
    case AIMP_EFFECT_CHANGED:
        event = EVENT_EFFECT_CHANGED;
        break;
    case AIMP_EQ_CHANGED:
        event = EVENT_EQ_CHANGED;
        break;
    default:
        {
            const CallbackIdNameMap& callback_names = aimp_manager->aimp_callback_names_;
            auto callback_name_it = callback_names.find(dwCBType);
            if ( callback_name_it != callback_names.end() ) {
                BOOST_LOG_SEV(logger(), info) << "callback " << callback_name_it->second << " is invoked";
            } else {
                BOOST_LOG_SEV(logger(), info) << "callback " << dwCBType << " is invoked";
            }
        }
        break;
    }

    if (event != EVENTS_COUNT) {
        aimp_manager->strand_.post( boost::bind(&AIMPManager26::notifyAllExternalListeners,
                                                aimp_manager,
                                                event
                                                )
                                   );
    }
}

void AIMPManager26::notifyAllExternalListeners(EVENTS event) const
{
    BOOST_FOREACH(const auto& listener_pair, external_listeners_) {
        const EventsListener& listener = listener_pair.second;
        listener(event);
    }
}

AIMPManager26::EventsListenerID AIMPManager26::registerListener(AIMPManager26::EventsListener listener)
{
    external_listeners_[next_listener_id_] = listener;
    assert(next_listener_id_ != UINT32_MAX);
    return ++next_listener_id_; // get next unique listener ID using simple increment.
}

void AIMPManager26::unRegisterListener(AIMPManager26::EventsListenerID listener_id)
{
    external_listeners_.erase(listener_id);
}

namespace
{

void clear(std::wostringstream& os)
{
    os.clear();
    os.str( std::wstring() );    
}

struct Formatter
{
    int column_index;
    Formatter(int column_index) : column_index(column_index) {}
    Formatter(const Formatter& rhs) : column_index(rhs.column_index) {}
};

struct BitrateFormatter : public Formatter
{
    mutable std::wostringstream os;

    BitrateFormatter(int column_index) : Formatter(column_index) {}

    // need to define copy ctor since wostringstream has no copy ctor.
    BitrateFormatter(const BitrateFormatter& rhs) : Formatter(rhs), os() {}

    std::wstring operator()(sqlite3_stmt* stmt) const { 
        clear(os);
        os << sqlite3_column_int(stmt, column_index) << L" kbps";
        return os.str();
    }
};

struct ChannelsCountFormatter : public Formatter
{
    mutable std::wostringstream os;

    ChannelsCountFormatter(int column_index) : Formatter(column_index) {}

    // need to define copy ctor since wostringstream has no copy ctor.
    ChannelsCountFormatter(const ChannelsCountFormatter& rhs) : Formatter(rhs), os() {}

    std::wstring operator()(sqlite3_stmt* stmt) const { 
        clear(os);

        const int channels_count = sqlite3_column_int(stmt, column_index);
        switch (channels_count) {
        case 0:
            break;
        case 1:
            os << L"Mono";
            break;
        case 2:
            os << L"Stereo";
            break;
        default:
            os << channels_count << L" channels";
            break;
        }
        return os.str();
    }
};

struct DurationFormatter : public Formatter
{
    mutable std::wostringstream os;

    DurationFormatter(int column_index) : Formatter(column_index) {}

    // need to define copy ctor since wostringstream has no copy ctor.
    DurationFormatter(const DurationFormatter& rhs) : Formatter(rhs), os() {}

    std::wstring operator()(sqlite3_stmt* stmt) const {
        clear(os);
        formatTime( os, sqlite3_column_int(stmt, column_index) );
        return os.str();
    }

    static void formatTime(std::wostringstream& os, DWORD input_time_ms)
    {
        const DWORD input_time_sec = input_time_ms / 1000;
        const DWORD time_hour = input_time_sec / 3600;
        const DWORD time_min = (input_time_sec % 3600) / 60;
        const DWORD time_sec = input_time_sec % 60;

        const wchar_t delimeter = L':';

        using namespace std;

        if (time_hour > 0) {
            os << setfill(L'0') << setw(2) << time_hour;
            os << delimeter;
        }
        os << setfill(L'0') << setw(2) << time_min;
        os << delimeter;
        os << setfill(L'0') << setw(2) << time_sec;
    }
};

struct FileNameExtentionFormatter : public Formatter
{
    FileNameExtentionFormatter(int column_index) : Formatter(column_index) {}

    std::wstring operator()(sqlite3_stmt* stmt) const {
        const auto filename = static_cast<const std::wstring::value_type*>( sqlite3_column_text16(stmt, column_index) );
        std::wstring ext = boost::filesystem::path(filename).extension().native();
        if ( !ext.empty() ) {
            if (ext[0] == L'.') {
                ext.erase( ext.begin() );
            }
        }
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        return ext;
    }
};

struct SampleRateFormatter : public Formatter 
{
    mutable std::wostringstream os;

    SampleRateFormatter(int column_index) : Formatter(column_index) {}

    // need to define copy ctor since wostringstream has no copy ctor.
    SampleRateFormatter(const SampleRateFormatter& rhs) : Formatter(rhs), os() {}

    std::wstring operator()(sqlite3_stmt* stmt) const { 
        clear(os);

        const int rate_in_hertz = sqlite3_column_int(stmt, column_index);
        os << (rate_in_hertz / 1000)
           << L" kHz";
        return os.str();
    }
};

struct FileSizeFormatter : public Formatter
{
    mutable std::wostringstream os;

    FileSizeFormatter(int column_index) : Formatter(column_index) {}

    // need to define copy ctor since wostringstream has no copy ctor.
    FileSizeFormatter(const FileSizeFormatter& rhs) : Formatter(rhs), os() {}

    std::wstring operator()(sqlite3_stmt* stmt) const {
        clear(os);
        formatSize( os, sqlite3_column_int64(stmt, column_index) );
        return os.str();
    }

    static void formatSize(std::wostringstream& os, INT64 size_in_bytes) {
        const INT64 bytes_in_megabyte = 1024 * 1024;
        os.precision(3);
        if (size_in_bytes >= bytes_in_megabyte) {
            os << size_in_bytes / double(bytes_in_megabyte) << L" Mb";
        } else {
            os << size_in_bytes / 1024.0 << L" kb";
        }
    }
};

struct RawStringFormatter : public Formatter
{
    RawStringFormatter(int column_index) : Formatter(column_index) {}
    std::wstring operator()(sqlite3_stmt* stmt) const {
        return static_cast<const std::wstring::value_type*>( sqlite3_column_text16(stmt, column_index) );
    }
};

struct RawIntFormatter : public Formatter
{
    RawIntFormatter(int column_index) : Formatter(column_index) {}
    std::wstring operator()(sqlite3_stmt* stmt) const {
        return boost::lexical_cast<std::wstring>( sqlite3_column_int(stmt, column_index) );
    }
};

/*!
    \brief Helper class for AIMPManager26::getFormattedEntryTitle() function.
           Implementation of AIMP title format analog.
*/
class PlaylistEntryTitleFormatter
{
public:
    typedef std::wstring::value_type char_t;

    PlaylistEntryTitleFormatter()
    {
        auto artist_formatter = boost::bind<std::wstring>(RawStringFormatter(ARTIST), _1);
        using namespace boost::assign;
        insert(formatters_)
            ( L'A', boost::bind<std::wstring>(RawStringFormatter(ALBUM), _1) )
            ( L'a', artist_formatter )
            ( L'B', boost::bind<std::wstring>(BitrateFormatter(BITRATE), _1) )
            ( L'C', boost::bind<std::wstring>(ChannelsCountFormatter(CHANNELS_COUNT), _1) )
            ( L'E', boost::bind<std::wstring>(FileNameExtentionFormatter(FILENAME), _1) )
            //( L'F', boost::bind<std::wstring>(RawStringFormatter(FILENAME), _1) ) getting filename is disabled.
            ( L'G', boost::bind<std::wstring>(RawStringFormatter(GENRE), _1) )
            ( L'H', boost::bind<std::wstring>(SampleRateFormatter(SAMPLERATE), _1) )
            ( L'L', boost::bind<std::wstring>(DurationFormatter(DURATION), _1) )
            ( L'M', boost::bind<std::wstring>(RawIntFormatter(RATING), _1) )
            ( L'R', artist_formatter ) // format R = a in AIMP3.
            ( L'S', boost::bind<std::wstring>(FileSizeFormatter(FILESIZE), _1) )
            ( L'T', boost::bind<std::wstring>(RawStringFormatter(TITLE), _1) )
            ( L'Y', boost::bind<std::wstring>(RawStringFormatter(DATE), _1) )
        ;
        formatters_end_ = formatters_.end();
    }

    static bool endOfFormatString(std::wstring::const_iterator curr_char,
                                  std::wstring::const_iterator end,
                                  char_t end_of_string
                                  )
    {
        return curr_char == end || *curr_char == end_of_string;
    }

    // returns count of characters read.
    size_t format(sqlite3_stmt* stmt,
                  std::wstring::const_iterator begin,
                  std::wstring::const_iterator end,
                  char_t end_of_string,
                  std::wstring& formatted_string) const
    {
        auto curr_char = begin;
        while ( !endOfFormatString(curr_char, end, end_of_string) ) {
            if (*curr_char == format_argument_symbol) {
                if (curr_char + 1 != end) {
                    ++curr_char;
                    const auto formatter_it = formatters_.find(*curr_char);
                    if (formatter_it != formatters_end_) {
                        formatted_string += formatter_it->second(stmt);
                        curr_char += 1; // go to char next to format argument.
                    } else {
                        switch(*curr_char) {
                        case L'%':
                        case L',': // since ',' and ')' chars are used in expression "%IF(a, b, c)" we must escape them in usual string as '%,' '%)'.
                        case L')':
                            formatted_string.push_back(*curr_char++);
                            break;
                        default:
                            {
                            if (*curr_char == L'I')
                                if (curr_char != end && *++curr_char == L'F')
                                    if (curr_char != end && *++curr_char == L'(') {
                                        // %IF(a, b, c): means a.empty() ? c : b;
                                        ++curr_char;
                                        std::wstring a;
                                        std::advance( curr_char, format(stmt, curr_char, end, L',', a) ); // read a.
                                        ++curr_char;

                                        std::wstring b;
                                        std::advance( curr_char, format(stmt, curr_char, end, L',', b) ); // read b.
                                        ++curr_char;

                                        std::wstring c;
                                        std::advance( curr_char, format(stmt, curr_char, end, L')', c) ); // read c.
                                        ++curr_char;

                                        formatted_string.append(a.empty() ? c : b);
                                        break;
                                    }
                            throw std::invalid_argument("Wrong format string: unknown format argument.");
                            }
                            break;
                        }
                    }
                } else {
                    throw std::invalid_argument("Wrong format string: malformed format argument.");
                }
            } else {
                formatted_string.push_back(*curr_char++);
            }
        }

        return static_cast<size_t>( std::distance(begin, curr_char) );
    }

    std::wstring format(TrackDescription track_desc, const std::wstring& format_string, sqlite3* playlists_db) const // throw std::invalid_argument
    {
        std::ostringstream query;
        auto f = [](ENTRY_FIELD_ID id) { return getField(id); };
        query << "SELECT "
              << f(ALBUM) << ',' << f(ARTIST) << ',' << f(DATE) << ',' << f(FILENAME) << ',' << f(GENRE) << ',' << f(TITLE) << ','
              << f(BITRATE) << ',' << f(CHANNELS_COUNT) << ',' << f(DURATION) << ',' << f(FILESIZE) << ',' << f(RATING) << ',' << f(SAMPLERATE)
              << " FROM PlaylistsEntries WHERE playlist_id=" << track_desc.playlist_id << " AND entry_id=" << track_desc.track_id;

        sqlite3_stmt* stmt = createStmt( playlists_db, query.str() );
        ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

        for(;;) {
		    int rc_db = sqlite3_step(stmt);
            if (SQLITE_ROW == rc_db) {
                const auto begin = format_string.begin(),
                           end   = format_string.end();
                std::wstring formatted_string;
                format(stmt, begin, end, L'\0', formatted_string);
                return formatted_string;
            } else if (SQLITE_DONE == rc_db) {
                break;
            } else {
                const std::string msg = MakeString() << "sqlite3_step() error "
                                                     << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                                     << ". Query: " << query.str();
                throw std::runtime_error(msg);
		    }
        }
        throw std::runtime_error(MakeString() << "Track " << track_desc << " not found at "__FUNCTION__);
    }

private:

    enum ENTRY_FIELD_ID {
        ALBUM = 0, ARTIST, DATE, FILENAME, GENRE, TITLE, BITRATE, CHANNELS_COUNT, DURATION, FILESIZE, RATING, SAMPLERATE, FIELDS_COUNT
    };
    static const char* getField(ENTRY_FIELD_ID field_index) {
        const char* fields[] = {"album", "artist", "date", "filename", "genre", "title", "bitrate", "channels_count", "duration", "filesize", "rating", "samplerate"};
        Utilities::AssertArraySize<ENTRY_FIELD_ID::FIELDS_COUNT>(fields);
        assert(field_index < FIELDS_COUNT);
        return fields[field_index];
    }

    typedef boost::function<std::wstring(sqlite3_stmt*)> EntryFieldStringGetter;
    typedef std::map<char_t, EntryFieldStringGetter> Formatters;
    Formatters formatters_;
    Formatters::const_iterator formatters_end_;

    static const char_t format_argument_symbol = L'%';

} playlistentry_title_formatter;

} // namespace anonymous

std::wstring AIMPManager26::getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const // throw std::invalid_argument
{
    return playlistentry_title_formatter.format(getAbsoluteTrackDesc(track_desc),
                                                StringEncoding::utf8_to_utf16(format_string_utf8),
                                                playlists_db_
                                                );
}

std::wstring AIMPManager26::getEntryFilename(TrackDescription track_desc) const // throw std::invalid_argument
{
    TrackDescription track_desc_ = getAbsoluteTrackDesc(track_desc);

    WCHAR filename[MAX_PATH + 1] = {0};
    boolean result = aimp2_playlist_manager_->AIMP_PLS_Entry_FileNameGet(track_desc_.playlist_id, track_desc_.track_id,
                                                                         &filename[0], ARRAYSIZE(filename) - 1 // tested: it requires string char count, not buffer size.
                                                                         );
    if (!result) {
        throw std::runtime_error(MakeString() << "IAIMPAddonsPlaylistManager::AIMP_PLS_Entry_FileNameGet() failed.");
    }
    return filename;
}

void AIMPManager26::initPlaylistDB() // throws std::runtime_error
{
#define THROW_IF_NOT_OK_WITH_MSG(rc, msg_expr)  if (SQLITE_OK != rc) { \
                                                    const std::string msg = msg_expr; \
                                                    shutdownPlaylistDB(); \
                                                    throw std::runtime_error(msg); \
                                                }

    int rc = sqlite3_open(":memory:", &playlists_db_);
    THROW_IF_NOT_OK_WITH_MSG( rc, MakeString() << "Playlist database creation failure. Reason: sqlite3_open error "
                                               << rc << ": " << sqlite3_errmsg(playlists_db_) );

    { // add case-insensitivity support to LIKE operator (used for search tracks) since default LIKE operator since it supports case-insensitive search only on ASCII chars by default).
    rc = sqlite3_unicode_init(playlists_db_);
    THROW_IF_NOT_OK_WITH_MSG( rc, MakeString() << "unicode support enabling failure. Reason: sqlite3_unicode_init() error "
                                               << rc << ": " << sqlite3_errmsg(playlists_db_) );
    }

    { // create table for content of all playlists.
    char* errmsg = nullptr;
    ON_BLOCK_EXIT(&sqlite3_free, errmsg);
    rc = sqlite3_exec(playlists_db_,
                      "CREATE TABLE PlaylistsEntries ( playlist_id    INTEGER,"
                                                      "entry_id       INTEGER,"
                                                      "entry_index    INTEGER," // for AIMP2 it is the same as entry_id.
                                                      "album          VARCHAR(128),"
                                                      "artist         VARCHAR(128),"
                                                      "date           VARCHAR(16),"
                                                      "filename       VARCHAR(260),"
                                                      "genre          VARCHAR(32),"
                                                      "title          VARCHAR(260),"
                                                      "bitrate        INTEGER,"
                                                      "channels_count INTEGER,"
                                                      "duration       INTEGER,"
                                                      "filesize       BIGINT,"
                                                      "rating         TINYINT,"
                                                      "samplerate     INTEGER,"
                                                      "crc32          BIGINT," // use BIGINT since crc32 is uint32.
                                                      "PRIMARY KEY (playlist_id, entry_id)"
                                                      ")",
                      nullptr, /* Callback function */
                      nullptr, /* 1st argument to callback */
                      &errmsg
                      );
    THROW_IF_NOT_OK_WITH_MSG( rc, MakeString() << "Playlist content table creation failure. Reason: sqlite3_exec(create table) error "
                                               << rc << ": " << errmsg );
    }

    { // create table for playlist.
    char* errmsg = nullptr;
    ON_BLOCK_EXIT(&sqlite3_free, errmsg);
    rc = sqlite3_exec(playlists_db_,
                      "CREATE TABLE Playlists ( id              INTEGER,"
                                               "playlist_index  INTEGER,"
                                               "title           VARCHAR(260),"
                                               "entries_count   INTEGER,"
                                               "duration        BIGINT,"
                                               "size_of_entries BIGINT,"
                                               "crc32           BIGINT," // use BIGINT since crc32 is uint32.
                                               "PRIMARY KEY (id)"
                                               ")",
                      nullptr, /* Callback function */
                      nullptr, /* 1st argument to callback */
                      &errmsg
                      );
    THROW_IF_NOT_OK_WITH_MSG( rc, MakeString() << "Playlist table creation failure. Reason: sqlite3_exec(create table) error "
                                               << rc << ": " << errmsg );
    }

#undef THROW_IF_NOT_OK_WITH_MSG
}

void AIMPManager26::shutdownPlaylistDB()
{
    const int rc = sqlite3_close(playlists_db_);
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << "sqlite3_close error: " << rc;
    }
    playlists_db_ = nullptr;
}

void AIMPManager26::deletePlaylistFromPlaylistDB(PlaylistID playlist_id)
{
    deletePlaylistEntriesFromPlaylistDB(playlist_id);

    const std::string query = MakeString() << "DELETE FROM Playlists WHERE id=" << playlist_id;

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(playlists_db_,
                                query.c_str(),
                                nullptr, /* Callback function */
                                nullptr, /* 1st argument to callback */
                                &errmsg
                                );
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << __FUNCTION__" failed. Reason: sqlite3_exec() error "
                                       << rc << ": " << errmsg
                                       << ". Query: " << query;
        sqlite3_free(errmsg);
    }
}

void AIMPManager26::deletePlaylistEntriesFromPlaylistDB(PlaylistID playlist_id)
{
    const std::string query = MakeString() << "DELETE FROM PlaylistsEntries WHERE playlist_id=" << playlist_id;

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(playlists_db_,
                                query.c_str(),
                                nullptr, /* Callback function */
                                nullptr, /* 1st argument to callback */
                                &errmsg
                                );
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << __FUNCTION__" failed. Reason: sqlite3_exec() error "
                                       << rc << ": " << errmsg 
                                       << ". Query: " << query;
        sqlite3_free(errmsg);
    }
}

void AIMPManager26::addFileToPlaylist(const boost::filesystem::wpath& path, PlaylistID playlist_id) // throws std::runtime_error
{
    AIMP2SDK::IPLSStrings* strings;   
    if (aimp2_controller_->AIMP_NewStrings(&strings)) {
        ON_BLOCK_EXIT_OBJ((*strings), &AIMP2SDK::IPLSStrings::Release);
        if (!strings->AddFile(const_cast<PWCHAR>( path.native().c_str() ), nullptr)) {
            throw std::runtime_error(MakeString() << "IPLSStrings::AddFile() failed.");
        }

        if (!aimp2_controller_->AIMP_PLS_AddFiles(getAbsolutePlaylistID(playlist_id), strings)) {
            throw std::runtime_error(MakeString() << "IAIMP2Controller::AIMP_PLS_AddFiles() failed.");
        }
    }
}

void AIMPManager26::addURLToPlaylist(const std::string& url, PlaylistID playlist_id) // throws std::runtime_error
{
    boost::filesystem::wpath path(StringEncoding::utf8_to_utf16(url));

    addFileToPlaylist(path, playlist_id);
}

PlaylistID AIMPManager26::createPlaylist(const std::wstring& title)
{
    return aimp2_playlist_manager_->AIMP_PLS_NewEx(const_cast<PWCHAR>(title.c_str()),
                                                   false
                                                   );
}

void AIMPManager26::removeTrack(TrackDescription track_desc, bool physically) // throws std::runtime_error
{
    if (physically) {
        BOOST_LOG_SEV(logger(), warning) << __FUNCTION__": AIMP2 SDK does not support track physical removing";
    }

    track_desc = getAbsoluteTrackDesc(track_desc);

    if (!aimp2_playlist_manager_->AIMP_PLS_Entry_Delete(track_desc.playlist_id, track_desc.track_id)) {
        throw std::runtime_error("IAIMP2PlaylistManager2::AIMP_PLS_Entry_Delete error.");
    }
}

} // namespace AIMPPlayer
