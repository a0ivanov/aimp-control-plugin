// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "manager3.1.h"
#include "plugin/logger.h"
#include "aimp3_util.h"
#include "utils/sqlite_util.h"
#include "sqlite/sqlite.h"
#include <boost/foreach.hpp>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}

namespace AIMPPlayer
{

using namespace Utilities;

AIMPManager31::AIMPManager31(boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit)
    :
    AIMPManager30(aimp3_core_unit)
{
    try {
        initializeAIMPObjects();

        initPlaylistDB();
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured during AIMPManager31 initialization. Reason:") + e.what() );
    }
}

AIMPManager31::~AIMPManager31()
{
}

void AIMPManager31::initializeAIMPObjects()
{
    using namespace AIMP3SDK;
    IAIMPAddonsPlaylistQueue* playlist_queue;
    if (S_OK != aimp3_core_unit_->QueryInterface(IID_IAIMPAddonsPlaylistQueue,
                                                 reinterpret_cast<void**>(&playlist_queue)
                                                 ) 
        )
    {
        throw std::runtime_error("Creation object IAIMPAddonsPlaylistQueue failed"); 
    }
    aimp3_playlist_queue_.reset(playlist_queue);
    playlist_queue->Release();
}

void AIMPManager31::reloadQueuedEntries() // throws std::runtime_error
{
    using namespace AIMP3SDK;
    // PROFILE_EXECUTION_TIME(__FUNCTION__);

    deleteQueuedEntriesFromPlaylistDB(); // remove old entries before adding new ones.

    sqlite3_stmt* stmt = createStmt(playlists_db_, "INSERT INTO QueuedEntries VALUES (?,?,?,?,?,"
                                                                                     "?,?,?,?,?,"
                                                                                     "?,?,?,?,?)"
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    AIMP3Util::FileInfoHelper file_info_helper; // used for get entries from AIMP conveniently.

    const int entries_count = aimp3_playlist_queue_->QueueEntryGetCount();
    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        HPLSENTRY entry_handle;
        HRESULT r = aimp3_playlist_queue_->QueueEntryGet(entry_index, &entry_handle);

        if (S_OK != r) {
            const std::string msg = MakeString() << "IAIMPAddonsPlaylistQueue::QueueEntryGet() error " 
                                                 << r << " occured while getting entry info ¹" << entry_index;
            throw std::runtime_error(msg);
        }

        r = aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP_PLAYLIST_ENTRY_PROPERTY_INFO,
                                                            &file_info_helper.getEmptyFileInfo(), sizeof(file_info_helper.getEmptyFileInfo())
                                                            );

        if (S_OK != r) {
            const std::string msg = MakeString() << "IAIMPAddonsPlaylistManager::EntryPropertyGetValue() error " 
                                                 << r << " occured while getting entry info ¹" << entry_index;
            throw std::runtime_error(msg);
        }

        { // get rating manually, since AIMP3 does not fill TAIMPFileInfo::Rating value.
            int rating = 0;
            r = aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_MARK, &rating, sizeof(rating) );    
            if (S_OK != r) {
                rating =  0;
            }

            // special db code
            {
#define bind(type, field_index, value)  rc_db = sqlite3_bind_##type(stmt, field_index, value); \
                                        if (SQLITE_OK != rc_db) { \
                                            const std::string msg = MakeString() << "Error sqlite3_bind_"#type << " " << rc_db; \
                                            throw std::runtime_error(msg); \
                                        }
#define bindText(field_index, info_field_name)  rc_db = sqlite3_bind_text16(stmt, field_index, info.##info_field_name##Buffer, info.##info_field_name##BufferSizeInChars * sizeof(WCHAR), SQLITE_STATIC); \
                                                if (SQLITE_OK != rc_db) { \
                                                    const std::string msg = MakeString() << "sqlite3_bind_text16" << " " << rc_db; \
                                                    throw std::runtime_error(msg); \
                                                }
                int rc_db;
                TrackDescription track_desc = getTrackDescOfQueuedEntry(entry_handle);
                bind(int,    1, track_desc.playlist_id);
                // bind all values
                const AIMP3SDK::TAIMPFileInfo& info = file_info_helper.getFileInfoWithCorrectStringLengths();
                bind(int,    2, track_desc.track_id);
                bind(int,    3, entry_index);
                bindText(    4, Album);
                bindText(    5, Artist);
                bindText(    6, Date);
                bindText(    7, FileName);
                bindText(    8, Genre);
                bindText(    9, Title);
                bind(int,   10, info.BitRate);
                bind(int,   11, info.Channels);
                bind(int,   12, info.Duration);
                bind(int64, 13, info.FileSize);
                bind(int,   14, rating);
                bind(int,   15, info.SampleRate);
                

                rc_db = sqlite3_step(stmt);
                if (SQLITE_DONE != rc_db) {
                    const std::string msg = MakeString() << "sqlite3_step() error "
                                                         << rc_db << ": " << sqlite3_errmsg(playlists_db_);
                    throw std::runtime_error(msg);
                }
                sqlite3_reset(stmt);
#undef bind
#undef bindText
            }
        }
    }
}

TrackDescription AIMPManager31::getTrackDescOfQueuedEntry(AIMP3SDK::HPLSENTRY entry_handle) const // throws std::runtime_error
{
    // We rely on fact that AIMP always have queued entry in one of playlists.

    const PlaylistEntryID entry_id = castToPlaylistEntryID(entry_handle);

    // find playlist id.
    const std::string& query = MakeString() << "SELECT playlist_id FROM PlaylistsEntries "
                                            << "WHERE entry_id = " << entry_id;

    sqlite3* playlists_db = playlists_db_;
    sqlite3_stmt* stmt = createStmt( playlists_db, query.c_str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            int playlist_id = sqlite3_column_int(stmt, 0);
            return TrackDescription(playlist_id, entry_id);
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                                 << ". Query: " << query;
            throw std::runtime_error(msg);
		}
    }

    throw std::runtime_error(MakeString() << "Queued entry is not found at existing playlists unexpectedly. Entry AIMP id: " << entry_id);
}

void AIMPManager31::moveQueueEntry(TrackDescription track_desc, int new_queue_index) // throws std::runtime_error
{
    AIMP3SDK::HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));
    HRESULT r = aimp3_playlist_queue_->QueueEntryMove(entry_handle, new_queue_index);
    if (S_OK != r) {
        const std::string msg = MakeString() << "IAIMPAddonsPlaylistQueue::QueueEntryMove() error " << r;
        throw std::runtime_error(msg);
    }
}

void AIMPManager31::moveQueueEntry(int old_queue_index, int new_queue_index) // throws std::runtime_error
{
    HRESULT r = aimp3_playlist_queue_->QueueEntryMove2(old_queue_index, new_queue_index);
    if (S_OK != r) {
        const std::string msg = MakeString() << "IAIMPAddonsPlaylistQueue::QueueEntryMove2() error " << r;
        throw std::runtime_error(msg);
    }
}

void AIMPManager31::enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) // throws std::runtime_error
{
    using namespace AIMP3SDK;
    AIMP3SDK::HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));

    HRESULT r = aimp3_playlist_queue_->QueueEntryAdd(entry_handle, insert_at_queue_beginning);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager31::removeEntryFromPlayQueue(TrackDescription track_desc) // throws std::runtime_error
{
    using namespace AIMP3SDK;
    AIMP3SDK::HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));

    HRESULT r = aimp3_playlist_queue_->QueueEntryRemove(entry_handle);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager31::initPlaylistDB() // throws std::runtime_error
{
#define THROW_IF_NOT_OK_WITH_MSG(rc, msg_expr)  if (SQLITE_OK != rc) { \
                                                    const std::string msg = msg_expr; \
                                                    throw std::runtime_error(msg); \
                                                }
    int rc;
    { // create table for content of entries queue.
    char* errmsg = nullptr;
    ON_BLOCK_EXIT(&sqlite3_free, errmsg);
    rc = sqlite3_exec(playlists_db_,
                      "CREATE TABLE QueuedEntries (  playlist_id    INTEGER,"
                                                    "entry_id       INTEGER,"
                                                    "queue_index    INTEGER,"
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
                                                    "PRIMARY KEY (entry_id)"
                                                  ")",
                      nullptr, /* Callback function */
                      nullptr, /* 1st argument to callback */
                      &errmsg
                      );
    THROW_IF_NOT_OK_WITH_MSG( rc, MakeString() << "Queue content table creation failure. Reason: sqlite3_exec(create table) error "
                                               << rc << ": " << errmsg );
    }
#undef THROW_IF_NOT_OK_WITH_MSG
}

namespace {
// On error it prints error reason to log only.
void executeQuery(const std::string& query, sqlite3* db, const char* log_tag)
{
    char* errmsg = nullptr;
    const int rc = sqlite3_exec(db,
                                query.c_str(),
                                nullptr, /* Callback function */
                                nullptr, /* 1st argument to callback */
                                &errmsg
                                );
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << log_tag << " failed. Reason: sqlite3_exec() error "
                                       << rc << ": " << errmsg 
                                       << ". Query: " << query;
        sqlite3_free(errmsg);
    }
}
} // namespace

void AIMPManager31::deleteQueuedEntriesFromPlaylistDB()
{
    const std::string query("DELETE FROM QueuedEntries");
    executeQuery(query, playlists_db_, __FUNCTION__);
}

} // namespace AIMPPlayer
