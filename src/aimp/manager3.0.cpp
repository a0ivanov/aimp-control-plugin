// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "manager3.0.h"
#include "manager_impl_common.h"
#include "aimp3_util.h"
#include "playlist_entry.h"
#include "utils/iunknown_impl.h"
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
#include <boost/filesystem.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}


/* Specialization of Utilities::crc32(T) for AIMP3SDK::TAIMPFileInfo struct.
     Be sure that following assertion is ok:
        PlaylistCRC32::crc32_entry() == crc32<AIMP3SDK::TAIMPFileInfo>()
*/
template<>
crc32_t Utilities::crc32<AIMP3SDK::TAIMPFileInfo>(const AIMP3SDK::TAIMPFileInfo& info)
{
#define ENTRY_FIELD_CRC32(field) (info.field##BufferSizeInChars == 0) ? 0 : Utilities::crc32( &info.field##Buffer[0], info.field##BufferSizeInChars * sizeof(info.field##Buffer[0]) )
    const crc32_t members_crc32_list [] = {
        ENTRY_FIELD_CRC32(Album),
        ENTRY_FIELD_CRC32(Artist),
        ENTRY_FIELD_CRC32(Date),
        ENTRY_FIELD_CRC32(FileName),
        ENTRY_FIELD_CRC32(Genre),
        ENTRY_FIELD_CRC32(Title),
        info.BitRate,
        info.Channels,
        info.Duration,
        Utilities::crc32(info.FileSize),
        info.Rating,
        info.SampleRate
    };
#undef ENTRY_FIELD_CRC32

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}


namespace AIMPPlayer
{

using namespace Utilities;

const AIMP3SDK::HPLS kInvalidPlaylistHandle = nullptr;
const int kNoParam1 = 0;
void * const kNoParam2 = nullptr;

template<>
PlaylistID cast(AIMP3SDK::HPLS handle)
{
    return reinterpret_cast<PlaylistID>(handle);
}

template<>
AIMP3SDK::HPLS cast(PlaylistID id)
{
    return reinterpret_cast<AIMP3SDK::HPLS>(id);
}

// use usual functions instead template specialization since in fact PlaylistEntryID == PlaylistID == int.
PlaylistEntryID castToPlaylistEntryID (AIMP3SDK::HPLSENTRY handle)
{
    return reinterpret_cast<PlaylistEntryID>(handle);
}

AIMP3SDK::HPLSENTRY castToHPLSENTRY(PlaylistEntryID id)
{
    return reinterpret_cast<AIMP3SDK::HPLSENTRY>(id);
}

class AIMPManager30::AIMPCoreUnitMessageHook : public IUnknownInterfaceImpl<AIMP3SDK::IAIMPCoreUnitMessageHook>
{
public:
    explicit AIMPCoreUnitMessageHook(AIMPManager30* aimp3_manager)
        : 
        aimp3_manager_(aimp3_manager)
    {}

    virtual void WINAPI CoreMessage(DWORD AMessage, int AParam1, void *AParam2, HRESULT *AResult) {
        aimp3_manager_->onAimpCoreMessage(AMessage, AParam1, AParam2, AResult);
    }

private:

    AIMPManager30* aimp3_manager_;
};

class AIMPManager30::AIMPAddonsPlaylistManagerListener : public IUnknownInterfaceImpl<AIMP3SDK::IAIMPAddonsPlaylistManagerListener>
{
public:
    explicit AIMPAddonsPlaylistManagerListener(AIMPManager30* aimp3_manager)
        : 
        aimp3_manager_(aimp3_manager),
        playlist_add_in_progress_(nullptr)
    {}

    virtual void WINAPI StorageActivated(AIMP3SDK::HPLS handle) {
        aimp3_manager_->onStorageActivated(handle);
    }
    virtual void WINAPI StorageAdded(AIMP3SDK::HPLS handle) {
        playlist_add_in_progress_ = handle;
        aimp3_manager_->onStorageAdded(handle);
        added_playlists_.insert(handle);
        if (store_change_args_) {
            aimp3_manager_->onStorageChanged(store_change_args_->handle, store_change_args_->flags);
            store_change_args_.reset();
        }
        playlist_add_in_progress_ = nullptr;
    }
    virtual void WINAPI StorageChanged(AIMP3SDK::HPLS handle, DWORD AFlags) {
        if ( playlistAdded(handle) ) {
            aimp3_manager_->onStorageChanged(handle, AFlags);
        } else {
            if (playlist_add_in_progress_ == handle) {
                if (!store_change_args_) {
                    store_change_args_ = boost::make_shared<StorageChangeArgs>(handle, AFlags);
                }
                store_change_args_->flags |= AFlags;
            }
        }
    }
    virtual void WINAPI StorageRemoved(AIMP3SDK::HPLS handle) {
        added_playlists_.erase(handle);
        aimp3_manager_->onStorageRemoved(handle);
    }

private:

    bool playlistAdded(AIMP3SDK::HPLS handle) const
        { return added_playlists_.find(handle) != added_playlists_.end(); }

    AIMPManager30* aimp3_manager_;
    typedef std::set<AIMP3SDK::HPLS> PlayListHandles;
    PlayListHandles added_playlists_;

    // properly load playlist in AIMP 3.20: it calls StorageChanged() from StorageAdded() function.
    // We should load entries after adding playlist manually.
    AIMP3SDK::HPLS playlist_add_in_progress_;

    struct StorageChangeArgs {
        AIMP3SDK::HPLS handle;
        DWORD flags;
        StorageChangeArgs(AIMP3SDK::HPLS handle, DWORD flags) : handle(handle), flags(flags) {}
    };
    boost::shared_ptr<StorageChangeArgs> store_change_args_;

};

AIMPManager30::AIMPManager30(boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit)
    :
    aimp3_core_unit_(aimp3_core_unit),
    next_listener_id_(0),
    playlists_db_(nullptr)
{
    try {
        initializeAIMPObjects();

        initPlaylistDB();

        // register listeners here

        aimp3_core_message_hook_.reset( new AIMPCoreUnitMessageHook(this) );
        // do not addref our pointer since AIMP do this itself. aimp3_core_message_hook_->AddRef();
        aimp3_core_unit_->MessageHook( aimp3_core_message_hook_.get() );

        aimp3_playlist_manager_listener_.reset( new AIMPAddonsPlaylistManagerListener(this)  );
        // do not addref our pointer since AIMP do this itself. aimp3_playlist_manager_listener_->AddRef();
        aimp3_playlist_manager_->ListenerAdd( aimp3_playlist_manager_listener_.get() );
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured during AIMPManager30 initialization. Reason:") + e.what() );
    }
}

AIMPManager30::~AIMPManager30()
{
    // unregister listeners here
    aimp3_playlist_manager_->ListenerRemove( aimp3_playlist_manager_listener_.get() );
    aimp3_playlist_manager_listener_.reset();

    aimp3_core_unit_->MessageUnhook( aimp3_core_message_hook_.get() );
    aimp3_core_message_hook_.reset();

    shutdownPlaylistDB();
}

void AIMPManager30::initializeAIMPObjects()
{
    using namespace AIMP3SDK;
    IAIMPAddonsPlayerManager* player_manager;
    if (S_OK != aimp3_core_unit_->QueryInterface(IID_IAIMPAddonsPlayerManager, 
                                                 reinterpret_cast<void**>(&player_manager)
                                                 ) 
        )
    {
        throw std::runtime_error("Creation object IAIMPAddonsPlayerManager failed"); 
    }
    aimp3_player_manager_.reset(player_manager);
    player_manager->Release();

    IAIMPAddonsPlaylistManager* playlist_manager;
    if (S_OK != aimp3_core_unit_->QueryInterface(IID_IAIMPAddonsPlaylistManager, 
                                                 reinterpret_cast<void**>(&playlist_manager)
                                                 ) 
        )
    {
        throw std::runtime_error("Creation object IAIMPAddonsPlaylistManager failed"); 
    }
    aimp3_playlist_manager_.reset(playlist_manager);
    playlist_manager->Release();

    IAIMPAddonsCoverArtManager* coverart_manager;
    if (S_OK != aimp3_core_unit_->QueryInterface(IID_IAIMPAddonsCoverArtManager, 
                                                 reinterpret_cast<void**>(&coverart_manager)
                                                 ) 
        )
    {
        throw std::runtime_error("Creation object IAIMPAddonsCoverArtManager failed"); 
    }
    aimp3_coverart_manager_.reset(coverart_manager);
    coverart_manager->Release();
}

void AIMPManager30::onTick()
{
}

void AIMPManager30::onStorageActivated(AIMP3SDK::HPLS /*handle*/)
{
    // do nothing, but if code will be added, it must not throw any exceptions, since this method called by AIMP.
}

void AIMPManager30::onStorageAdded(AIMP3SDK::HPLS handle)
{
    try {
        BOOST_LOG_SEV(logger(), debug) << "onStorageAdded: id = " << cast<PlaylistID>(handle);
        loadPlaylist(handle);
        notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__ << " for playlist with handle " << handle << ". Reason: " << e.what();
    } catch (...) {
        // we can't propagate exception from here since it is called from AIMP. Just log unknown error.
        BOOST_LOG_SEV(logger(), error) << "Unknown exception in "__FUNCTION__ << " for playlist with handle " << handle;
    }
}

std::string playlistNotifyFlagsToString(DWORD flags)
{
    using namespace AIMP3SDK;

#ifndef NDEBUG
    const DWORD all_flags =   AIMP_PLAYLIST_NOTIFY_NAME      | AIMP_PLAYLIST_NOTIFY_SELECTION  | AIMP_PLAYLIST_NOTIFY_TRACKINGINDEX
                            | AIMP_PLAYLIST_NOTIFY_PLAYINDEX | AIMP_PLAYLIST_NOTIFY_FOCUSINDEX | AIMP_PLAYLIST_NOTIFY_CONTENT
                            | AIMP_PLAYLIST_NOTIFY_ENTRYINFO | AIMP_PLAYLIST_NOTIFY_STATISTICS | AIMP_PLAYLIST_NOTIFY_PLAYINGSWITCHS
                            | AIMP_PLAYLIST_NOTIFY_READONLY  | AIMP_PLAYLIST_NOTIFY_PREIMAGE;
    assert(flags <= all_flags);
#endif

    const DWORD flag_count = 11;
    static const char * const strings[flag_count] = { "NAME",      "SELECTION",  "TRACKINGINDEX",
                                                      "PLAYINDEX", "FOCUSINDEX", "CONTENT",
                                                      "ENTRYINFO", "STATISTICS", "PLAYINGSWITCHS",
                                                      "READONLY",  "PREIMAGE"
                                                     };
    std::ostringstream os;
    if (flags) {
        for (DWORD f = flags, index = 0; f != 0; ++index, f >>= 1) {
            if ( (f & 0x1) == 0x1 ) {
                if ( os.tellp() != std::streampos(0) ) {
                    os << '|';
                }
                if (index < flag_count) {
                    os << strings[index];
                } else {
                    os << "out of range value";
                }
            }
        }
    } else {
        os << "NONE";
    }
    return os.str();
}

void AIMPManager30::onStorageChanged(AIMP3SDK::HPLS handle, DWORD flags)
{
    using namespace AIMP3SDK;

    try {
        BOOST_LOG_SEV(logger(), debug) << "onStorageChanged()...: id = " << cast<PlaylistID>(handle) << ", flags = " << flags << ": " << playlistNotifyFlagsToString(flags);

        PlaylistID playlist_id = cast<PlaylistID>(handle);
        bool is_playlist_changed = false;
        if (   (AIMP_PLAYLIST_NOTIFY_NAME       & flags) != 0 
            || (AIMP_PLAYLIST_NOTIFY_ENTRYINFO  & flags) != 0
            || (AIMP_PLAYLIST_NOTIFY_STATISTICS & flags) != 0 
            )
        {
            BOOST_LOG_SEV(logger(), debug) << "updatePlaylist";
            is_playlist_changed = true;
        }

        if (   (AIMP_PLAYLIST_NOTIFY_ENTRYINFO & flags) != 0  
            || (AIMP_PLAYLIST_NOTIFY_CONTENT   & flags) != 0 
            )
        {
            BOOST_LOG_SEV(logger(), debug) << "loadEntries";
            loadEntries(playlist_id); 
            is_playlist_changed = true;
        }

        if (is_playlist_changed) {
            updatePlaylistCrcInDB(playlist_id, getPlaylistCRC32(playlist_id));
            notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
        }

        BOOST_LOG_SEV(logger(), debug) << "...onStorageChanged()";
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__ << " for playlist with handle " << handle << ". Reason: " << e.what();
    } catch (...) {
        // we can't propagate exception from here since it is called from AIMP. Just log unknown error.
        BOOST_LOG_SEV(logger(), error) << "Unknown exception in "__FUNCTION__ << " for playlist with handle " << handle;
    }
}

void AIMPManager30::onStorageRemoved(AIMP3SDK::HPLS handle)
{
    try {
        const int playlist_id = cast<PlaylistID>(handle);
        playlist_crc32_list_.erase(playlist_id);
        deletePlaylistFromPlaylistDB(playlist_id);
        notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__ << " for playlist with handle " << handle << ". Reason: " << e.what();
    } catch (...) {
        // we can't propagate exception from here since it is called from AIMP. Just log unknown error.
        BOOST_LOG_SEV(logger(), error) << "Unknown exception in "__FUNCTION__ << " for playlist with handle " << handle;
    }

}

void AIMPManager30::loadPlaylist(int playlist_index)
{
    const char * const error_prefix = "Error occured while extracting playlist data: ";
    
    AIMP3SDK::HPLS handle = aimp3_playlist_manager_->StorageGet(playlist_index);
    if (handle == kInvalidPlaylistHandle) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPAddonsPlaylistManager::StorageGet failed");
    }

    return loadPlaylist(handle);
}

void AIMPManager30::loadPlaylist(AIMP3SDK::HPLS handle)
{
    const PlaylistID playlist_id = cast<PlaylistID>(handle);

    { // handle crc32.
    auto it = playlist_crc32_list_.find(playlist_id);
    if (it == playlist_crc32_list_.end()) {
        it = playlist_crc32_list_.insert(std::make_pair(playlist_id,
                                                        PlaylistCRC32(playlist_id, playlists_db_)
                                                        )
                                         ).first;
    }
    it->second.reset_properties();
    }

    const char * const error_prefix = "Error occured while extracting playlist data: ";
    
    using namespace AIMP3SDK;
  
    HRESULT r;
    INT64 duration;
    r = aimp3_playlist_manager_->StoragePropertyGetValue( handle, AIMP_PLAYLIST_STORAGE_PROPERTY_DURATION, &duration, sizeof(duration) );
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPAddonsPlaylistManager::StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_DURATION) failed. Result " << r);
    }

    INT64 size;
    r = aimp3_playlist_manager_->StoragePropertyGetValue( handle, AIMP_PLAYLIST_STORAGE_PROPERTY_SIZE, &size, sizeof(size) );
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPAddonsPlaylistManager::StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_SIZE) failed. Result " << r);
    }

    const size_t name_length = 256;
    WCHAR name[name_length + 1] = {0};
    r = aimp3_playlist_manager_->StoragePropertyGetValue(handle, AIMP_PLAYLIST_STORAGE_PROPERTY_NAME, name, name_length);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPAddonsPlaylistManager::StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_NAME) failed. Result " << r);
    }

    const int entries_count = aimp3_playlist_manager_->StorageGetEntryCount(handle);

    { // db code
    sqlite3_stmt* stmt = createStmt(playlists_db_,
                                    "REPLACE INTO Playlists VALUES (?,?,?,?,?,?)"
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
    bind(int,   1, playlist_id);
    bindText(   2, name, wcslen(name) );
    bind(int,   3, entries_count);
    bind(int64, 4, duration);
    bind(int64, 5, size);
    bind(int64, 6, kCRC32_UNINITIALIZED);
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

PlaylistCRC32& AIMPManager30::getPlaylistCRC32Object(PlaylistID playlist_id) const // throws std::runtime_error
{
    auto it = playlist_crc32_list_.find(playlist_id);
    if (it != playlist_crc32_list_.end()) {
        return it->second;
    }
    throw std::runtime_error(MakeString() << "Playlist " << playlist_id << " was not found in "__FUNCTION__);
}

crc32_t AIMPManager30::getPlaylistCRC32(PlaylistID playlist_id) const // throws std::runtime_error
{
    return getPlaylistCRC32Object(playlist_id).crc32();
}

void AIMPManager30::updatePlaylistCrcInDB(PlaylistID playlist_id, crc32_t crc32) // throws std::runtime_error
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
    bind(int,   2, playlist_id);
    
#undef bind
    rc_db = sqlite3_step(stmt);
    if (SQLITE_DONE != rc_db) {
        const std::string msg = MakeString() << "sqlite3_step() error "
                                             << rc_db << ": " << sqlite3_errmsg(playlists_db_);
        throw std::runtime_error(msg);
    }
}

void AIMPManager30::loadEntries(PlaylistID playlist_id) // throws std::runtime_error
{
    { // handle crc32.
        try {
            getPlaylistCRC32Object(playlist_id).reset_entries();
        } catch (std::exception& e) {
            throw std::runtime_error(MakeString() << "expected crc32 struct for playlist " << playlist_id << " not found in "__FUNCTION__". Reason: " << e.what());
        }
    }

    using namespace AIMP3SDK;
    // PROFILE_EXECUTION_TIME(__FUNCTION__);

    AIMP3Util::FileInfoHelper file_info_helper; // used for get entries from AIMP conveniently.

    const AIMP3SDK::HPLS playlist_handle = cast<AIMP3SDK::HPLS>(playlist_id);
    const int entries_count = aimp3_playlist_manager_->StorageGetEntryCount(playlist_handle);

    deletePlaylistEntriesFromPlaylistDB(playlist_id); // remove old entries before adding new ones.

    sqlite3_stmt* stmt = createStmt(playlists_db_, "INSERT INTO PlaylistsEntries VALUES (?,?,?,?,?,"
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
#define bindText(field_index, info_field_name)  rc_db = sqlite3_bind_text16(stmt, field_index, info.##info_field_name##Buffer, info.##info_field_name##BufferSizeInChars * sizeof(WCHAR), SQLITE_STATIC); \
                                                if (SQLITE_OK != rc_db) { \
                                                    const std::string msg = MakeString() << "sqlite3_bind_text16" << " " << rc_db; \
                                                    throw std::runtime_error(msg); \
                                                }

    int rc_db;
    bind(int, 1, playlist_id);
    
    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        const HPLSENTRY entry_handle = aimp3_playlist_manager_->StorageGetEntry(playlist_handle, entry_index);

        HRESULT r = aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_INFO,
                                                                    &file_info_helper.getEmptyFileInfo(), sizeof(file_info_helper.getEmptyFileInfo()) );
        if (S_OK != r) {
            const std::string msg = MakeString() << "IAIMPAddonsPlaylistManager::EntryPropertyGetValue(AIMP_PLAYLIST_ENTRY_PROPERTY_INFO) error " 
                                                 << r << " occured while getting entry info ¹" << entry_index
                                                 << " from playlist with ID = " << playlist_id;
            throw std::runtime_error(msg);
        }

        const int entry_id = castToPlaylistEntryID(entry_handle);

        int rating = 0;
        { // get rating manually, since AIMP3 does not fill TAIMPFileInfo::Rating value.
            r = aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_MARK, &rating, sizeof(rating) );    
            if (S_OK != r) {
                rating = 0;
            }
        }

        { // special db code
            // bind all values
            const AIMP3SDK::TAIMPFileInfo& info = file_info_helper.getFileInfoWithCorrectStringLengths();
            bind(int,    2, entry_id);
            bindText(    3, Album);
            bindText(    4, Artist);
            bindText(    5, Date);
            bindText(    6, FileName);
            bindText(    7, Genre);
            bindText(    8, Title);
            bind(int,    9, info.BitRate);
            bind(int,   10, info.Channels);
            bind(int,   11, info.Duration);
            bind(int64, 12, info.FileSize);
            bind(int,   13, rating);
            bind(int,   14, info.SampleRate);
            bind(int64, 15, crc32(info));

            rc_db = sqlite3_step(stmt);
            if (SQLITE_DONE != rc_db) {
                const std::string msg = MakeString() << "sqlite3_step() error "
                                                     << rc_db << ": " << sqlite3_errmsg(playlists_db_);
                throw std::runtime_error(msg);
            }
            sqlite3_reset(stmt);
        }
    }
#undef bind
#undef bindText
}

void AIMPManager30::startPlayback()
{
    // play current track.
    //aimp3_player_manager_->PlayStorage(aimp3_playlist_manager_->StoragePlayingGet(), -1); //aimp2_player_->PlayOrResume();
    using namespace AIMP3SDK;
    aimp3_core_unit_->MessageSend(AIMP_MSG_CMD_PLAY, kNoParam1, kNoParam2);
}

void AIMPManager30::startPlayback(TrackDescription track_desc) // throws std::runtime_error
{
    HRESULT r = aimp3_player_manager_->PlayEntry( castToHPLSENTRY( getAbsoluteEntryID(track_desc.track_id) ) );
    if (S_OK != r) {
        throw std::runtime_error( MakeString() << "Error " << r << " in "__FUNCTION__" with " << track_desc );
    }
}

void AIMPManager30::stopPlayback()
{
    using namespace AIMP3SDK;
    aimp3_core_unit_->MessageSend(AIMP_MSG_CMD_STOP, kNoParam1, kNoParam2); // aimp2_player_->Stop();
}

void AIMPManager30::pausePlayback()
{
    using namespace AIMP3SDK;
    aimp3_core_unit_->MessageSend(AIMP_MSG_CMD_PLAYPAUSE, kNoParam1, kNoParam2); // aimp2_player_->Pause();
}

void AIMPManager30::playNextTrack()
{
    using namespace AIMP3SDK;
    aimp3_core_unit_->MessageSend(AIMP_MSG_CMD_NEXT, kNoParam1, kNoParam2); // aimp2_player_->NextTrack();
}

void AIMPManager30::playPreviousTrack()
{
    using namespace AIMP3SDK;
    aimp3_core_unit_->MessageSend(AIMP_MSG_CMD_PREV, kNoParam1, kNoParam2); // aimp2_player_->PrevTrack();
}

void AIMPManager30::onAimpCoreMessage(DWORD AMessage, int AParam1, void* /*AParam2*/, HRESULT* AResult)
{
    using namespace AIMP3SDK;

    assert(AResult);
    /*
        ///!!! do not know when to notify about these events.
        EVENT_INFO_UPDATE,
        EVENT_EFFECT_CHANGED,
    */
    switch (AMessage) {
    case AIMP_MSG_EVENT_PLAYER_STATE:
        notifyAllExternalListeners(EVENT_PLAYER_STATE);
        break;
    case AIMP_MSG_EVENT_PLAYER_UPDATE_POSITION:
        //notifyAllExternalListeners(EVENT_PLAY_FILE);
        break;
    case AIMP_MSG_EVENT_STREAM_START:
        notifyAllExternalListeners(EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY);
        break;
    case AIMP_MSG_CMD_QUIT:
        notifyAllExternalListeners(EVENT_AIMP_QUIT);
        break;
    case AIMP_MSG_EVENT_PROPERTY_VALUE: {
        const int property_id = AParam1;
        switch (property_id) {
        case AIMP_MSG_PROPERTY_VOLUME:
            notifyAllExternalListeners(EVENT_VOLUME);
            break;
        case AIMP_MSG_PROPERTY_MUTE:
            notifyAllExternalListeners(EVENT_MUTE);
            break;
        case AIMP_MSG_PROPERTY_SHUFFLE:
            notifyAllExternalListeners(EVENT_SHUFFLE);
            break;
        case AIMP_MSG_PROPERTY_REPEAT:
            notifyAllExternalListeners(EVENT_REPEAT);
            break;
        case AIMP_MSG_PROPERTY_EQUALIZER:
        case AIMP_MSG_PROPERTY_EQUALIZER_BAND:
            notifyAllExternalListeners(EVENT_EQ_CHANGED);
            break;
        case AIMP_MSG_PROPERTY_PLAYER_POSITION:
            notifyAllExternalListeners(EVENT_TRACK_POS_CHANGED);
            break;
        case AIMP_MSG_PROPERTY_RADIOCAP:
            notifyAllExternalListeners(EVENT_RADIO_CAPTURE);
            break;
        default:
            notifyAllExternalListeners(EVENT_STATUS_CHANGE);
            break;
        }
        break;
    }
    default:
        AMessage = AMessage;
        break;
    }
    *AResult = E_NOTIMPL;
}

void AIMPManager30::setStatus(AIMPManager::STATUS status, AIMPManager::StatusValue status_value)
{
    //try {
    //    if ( FALSE == aimp2_controller_->AIMP_Status_Set(cast<AIMP2SDK_STATUS>(status), value) ) {
    //        throw std::runtime_error(MakeString() << "Error occured while setting status " << asString(status) << " to value " << value);
    //    }
    //} catch (std::bad_cast& e) {
    //    throw std::runtime_error( e.what() );
    //}

    //notifyAboutInternalEventOnStatusChange(status);

    using namespace AIMP3SDK;
    DWORD msg = 0;
    const int param1 = AIMP_MSG_PROPVALUE_SET;
    HRESULT r = S_OK;
    switch (status) {
    case STATUS_VOLUME: {
        msg = AIMP_MSG_PROPERTY_VOLUME;
        float value = status_value / 100.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_BALANCE: {
        msg = AIMP_MSG_PROPERTY_BALANCE;
        float value = status_value / 50.f - 1.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_SPEED: {
        msg = AIMP_MSG_PROPERTY_SPEED;
        float value = status_value / 100.f + 0.5f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_Player: {
        msg = AIMP_MSG_PROPERTY_PLAYER_STATE;
        int value = 0;
        switch (status_value) {
        case STOPPED: value = 0; break;
        case PAUSED:  value = 1; break;
        case PLAYING: value = 2; break;
        default:
            throw std::runtime_error( MakeString() << "Failed to set status: STATUS_Player. Reason: failed to convert status value " << status_value << " to AIMP3 value.");
        }
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_MUTE: {
        msg = AIMP_MSG_PROPERTY_MUTE;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_REVERB: {
        msg = AIMP_MSG_PROPERTY_REVERB;
        float value = status_value / 100.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_ECHO: {
        msg = AIMP_MSG_PROPERTY_ECHO;
        float value = status_value / 100.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_CHORUS: {
        msg = AIMP_MSG_PROPERTY_CHORUS;
        float value = status_value / 100.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_Flanger: {
        msg = AIMP_MSG_PROPERTY_FLANGER;
        float value = status_value / 100.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_EQ_STS: {
        msg = AIMP_MSG_PROPERTY_EQUALIZER;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_EQ_SLDR01:
    case STATUS_EQ_SLDR02:
    case STATUS_EQ_SLDR03:
    case STATUS_EQ_SLDR04:
    case STATUS_EQ_SLDR05:
    case STATUS_EQ_SLDR06:
    case STATUS_EQ_SLDR07:
    case STATUS_EQ_SLDR08:
    case STATUS_EQ_SLDR09:
    case STATUS_EQ_SLDR10:
    case STATUS_EQ_SLDR11:
    case STATUS_EQ_SLDR12:
    case STATUS_EQ_SLDR13:
    case STATUS_EQ_SLDR14:
    case STATUS_EQ_SLDR15:
    case STATUS_EQ_SLDR16:
    case STATUS_EQ_SLDR17:
    case STATUS_EQ_SLDR18: {
        msg = AIMP_MSG_PROPERTY_EQUALIZER_BAND;
        const int param1 = MAKELONG(status - STATUS_EQ_SLDR01, AIMP_MSG_PROPVALUE_SET);
        float value = status_value / 100.f * 30.f - 15.f;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_REPEAT: {
        msg = AIMP_MSG_PROPERTY_REPEAT;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_STAY_ON_TOP: {
        msg = AIMP_MSG_PROPERTY_STAYONTOP;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;                         
    }
    case STATUS_POS: {
        msg = AIMP_MSG_PROPERTY_PLAYER_POSITION;
        float value = static_cast<float>(status_value);
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            notifyAllExternalListeners(EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY);
            return;
        }
        break;
    }
    case STATUS_LENGTH:
    case STATUS_KBPS:
    case STATUS_KHZ: {
        throw std::runtime_error( MakeString() << "Failed to set read-only status: " << asString(status) );
    }
    case STATUS_ACTION_ON_END_OF_PLAYLIST: {
        msg = AIMP_MSG_PROPERTY_ACTION_ON_END_OF_PLAYLIST;
        int value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            enum { JUMP_TO_THE_NEXT_PLAYLIST,
                   REPEAT_PLAYLIST,
                   DO_NOTHING };
            return;
        }
        break;
    }
    case STATUS_REPEAT_SINGLE_FILE_PLAYLISTS: {
        msg = AIMP_MSG_PROPERTY_REPEAT_SINGLE_FILE_PLAYLISTS;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    //STATUS_MODE,
    //STATUS_STREAM_TYPE,
    case STATUS_REVERSETIME:
    case STATUS_SHUFFLE:
    case STATUS_RADIO_CAPTURE: {
        switch (status) {
        case STATUS_REVERSETIME:   msg = AIMP_MSG_PROPERTY_REVERSETIME; break;
        case STATUS_SHUFFLE:       msg = AIMP_MSG_PROPERTY_SHUFFLE;     break;
        case STATUS_RADIO_CAPTURE: msg = AIMP_MSG_PROPERTY_RADIOCAP;    break;
        default:
            assert(!"unknown status");
            break;
        }
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    case STATUS_MAIN_HWND:
    case STATUS_TC_HWND:
    case STATUS_APP_HWND:
    case STATUS_PL_HWND:
    case STATUS_EQ_HWND: {
        throw std::runtime_error( MakeString() << "Failed to set read-only status: STATUS_LENGTH");
        break;
    }
    case STATUS_TRAY: {
        msg = AIMP_MSG_PROPERTY_MINIMIZED_TO_TRAY;
        BOOL value = status_value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return;
        }
        break;
    }
    default:
        break;
    }

    std::ostringstream os;
    os << "Failed to set status " << status << ". Reason: ";
    if (r != S_OK) {
        os << "IAIMPCoreUnit::MessageSend(" << msg << ", " << param1 << ") returned " << r << ".";
    } else {
        os << "status is not supported.";
    }
    throw std::runtime_error( os.str() );
}

// For some reason IAIMPCoreUnit::MessageSend() returns -1 instead 1 for BOOL.
AIMPManager30::StatusValue patchBool(BOOL value)
{
    return value ? 1 : 0;
}

AIMPManager30::StatusValue AIMPManager30::getStatus(AIMPManager30::STATUS status) const
{
    //return aimp2_controller_->AIMP_Status_Get(status);
    using namespace AIMP3SDK;
    DWORD msg = 0;
    const int param1 = AIMP_MSG_PROPVALUE_GET;
    HRESULT r = S_OK;
    switch (status) {
    case STATUS_VOLUME: {
        msg = AIMP_MSG_PROPERTY_VOLUME;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value * 100.f);
        }
        break;
    }
    case STATUS_BALANCE: {
        msg = AIMP_MSG_PROPERTY_BALANCE;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>((value + 1.f)*50.f);
        }
        break;
    }
    case STATUS_SPEED: {
        msg = AIMP_MSG_PROPERTY_SPEED;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>((value - 0.5f) * 100.f);
        }
        break;
    }
    case STATUS_Player: {
        msg = AIMP_MSG_PROPERTY_PLAYER_STATE;
        int value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            switch (value) {
            case 0: return STOPPED;
            case 1: return PAUSED;
            case 2: return PLAYING;
            }
        }
        break;
    }
    case STATUS_MUTE: {
        msg = AIMP_MSG_PROPERTY_MUTE;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    case STATUS_REVERB: {
        msg = AIMP_MSG_PROPERTY_REVERB;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value * 100.f);
        }
        break;
    }
    case STATUS_ECHO: {
        msg = AIMP_MSG_PROPERTY_ECHO;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value * 100.f);
        }
        break;
    }
    case STATUS_CHORUS: {
        msg = AIMP_MSG_PROPERTY_CHORUS;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value * 100.f);
        }
        break;
    }
    case STATUS_Flanger: {
        msg = AIMP_MSG_PROPERTY_FLANGER;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value * 100.f);
        }
        break;
    }
    case STATUS_EQ_STS: {
        msg = AIMP_MSG_PROPERTY_EQUALIZER;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    case STATUS_EQ_SLDR01:
    case STATUS_EQ_SLDR02:
    case STATUS_EQ_SLDR03:
    case STATUS_EQ_SLDR04:
    case STATUS_EQ_SLDR05:
    case STATUS_EQ_SLDR06:
    case STATUS_EQ_SLDR07:
    case STATUS_EQ_SLDR08:
    case STATUS_EQ_SLDR09:
    case STATUS_EQ_SLDR10:
    case STATUS_EQ_SLDR11:
    case STATUS_EQ_SLDR12:
    case STATUS_EQ_SLDR13:
    case STATUS_EQ_SLDR14:
    case STATUS_EQ_SLDR15:
    case STATUS_EQ_SLDR16:
    case STATUS_EQ_SLDR17:
    case STATUS_EQ_SLDR18: {
        msg = AIMP_MSG_PROPERTY_EQUALIZER_BAND;
        const int param1 = MAKELONG(status - STATUS_EQ_SLDR01, AIMP_MSG_PROPVALUE_GET);
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>((value + 15.f) / 30.f * 100.f);
        }
        break;
    }
    case STATUS_REPEAT: {
        msg = AIMP_MSG_PROPERTY_REPEAT;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    case STATUS_STAY_ON_TOP: {
        msg = AIMP_MSG_PROPERTY_STAYONTOP;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;                         
    }
    case STATUS_POS: {
        msg = AIMP_MSG_PROPERTY_PLAYER_POSITION;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value);
        }
        break;
    }
    case STATUS_LENGTH: {
        msg = AIMP_MSG_PROPERTY_PLAYER_DURATION;
        float value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return static_cast<StatusValue>(value);
        }
        break;
    }
    case STATUS_ACTION_ON_END_OF_PLAYLIST: {
        msg = AIMP_MSG_PROPERTY_ACTION_ON_END_OF_PLAYLIST;
        int value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            enum { JUMP_TO_THE_NEXT_PLAYLIST,
                   REPEAT_PLAYLIST,
                   DO_NOTHING };
            
            return value;
        }
        break;
    }
    case STATUS_REPEAT_SINGLE_FILE_PLAYLISTS: {
        msg = AIMP_MSG_PROPERTY_REPEAT_SINGLE_FILE_PLAYLISTS;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    //STATUS_MODE,
    case STATUS_KBPS:
    case STATUS_KHZ: {
        const char* field = status == STATUS_KBPS ? "bitrate" : "samplerate";
        return getEntryField<DWORD>(playlists_db_, field, getPlayingEntry());
        }
        break;
    //STATUS_STREAM_TYPE,
    case STATUS_REVERSETIME:
    case STATUS_SHUFFLE:
    case STATUS_RADIO_CAPTURE: {
        switch (status) {
        case STATUS_REVERSETIME:   msg = AIMP_MSG_PROPERTY_REVERSETIME; break;
        case STATUS_SHUFFLE:       msg = AIMP_MSG_PROPERTY_SHUFFLE;     break;
        case STATUS_RADIO_CAPTURE: msg = AIMP_MSG_PROPERTY_RADIOCAP;    break;
        default:
            assert(!"unknown status");
            break;
        }
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    case STATUS_MAIN_HWND:
    case STATUS_TC_HWND:
    case STATUS_APP_HWND:
    case STATUS_PL_HWND:
    case STATUS_EQ_HWND: {
        msg = AIMP_MSG_PROPERTY_HWND;
        int param1 = 0;
        switch (status) {
        case STATUS_MAIN_HWND: param1 = AIMP_MPH_MAINFORM; break;
        case STATUS_APP_HWND:  param1 = AIMP_MPH_APPLICATION; break;
        case STATUS_TC_HWND:   param1 = AIMP_MPH_TRAYCONTROL; break;
        case STATUS_PL_HWND:   param1 = AIMP_MPH_PLAYLISTFORM; break;
        case STATUS_EQ_HWND:   param1 = AIMP_MPH_EQUALIZERFORM; break;
        default:
            assert(!"unknown status");
            break;
        }
        HWND value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return reinterpret_cast<StatusValue>(value);
        }
        break;
    }
    case STATUS_TRAY: {
        msg = AIMP_MSG_PROPERTY_MINIMIZED_TO_TRAY;
        BOOL value;
        r = aimp3_core_unit_->MessageSend(msg, param1, &value);
        if (S_OK == r) {
            return patchBool(value);
        }
        break;
    }
    default:
        break;
    }

    std::ostringstream os;
    os << "Failed to get status " << status << ". Reason: ";
    if (r != S_OK) {
        os << "IAIMPCoreUnit::MessageSend(" << msg << ", " << param1 << ") returned " << r << ".";
    } else {
        os << "status is not supported.";
    }
    throw std::runtime_error( os.str() );
}

std::string AIMPManager30::getAIMPVersion() const
{
    using namespace AIMP3SDK;
    TAIMPVersionInfo version_info = {0};
    HRESULT r = aimp3_core_unit_->GetVersion(&version_info);

    if (S_OK != r) {
        BOOST_LOG_SEV(logger(), error) << "IAIMPCoreUnit::GetVersion returned " << r;
        return "";
    }
    
    const int version = version_info.ID;
    using namespace std;
    ostringstream os;
    os << version / 1000 << '.' << setfill('0') << setw(2) << (version % 1000) / 10 << '.' << version % 10
       << " Build " << version_info.BuildNumber;    
    if (version_info.BuildSuffix) {
        os << ' ' << StringEncoding::utf16_to_system_ansi_encoding_safe(version_info.BuildSuffix);
    }

    std::string result(os.str());
    boost::algorithm::trim(result);
    return result;
}

PlaylistID AIMPManager30::getAbsolutePlaylistID(PlaylistID id) const
{
    if (id == -1) { // treat ID -1 as playing playlist.
        id = getPlayingPlaylist();
    }
    return id;
}

PlaylistEntryID AIMPManager30::getAbsoluteEntryID(PlaylistEntryID id) const // throws std::runtime_error
{
    if (id == -1) { // treat ID -1 as playing entry.
        id = getPlayingEntry();
    }

    return id;
}

TrackDescription AIMPManager30::getAbsoluteTrackDesc(TrackDescription track_desc) const // throws std::runtime_error
{
    return TrackDescription( getAbsolutePlaylistID(track_desc.playlist_id), getAbsoluteEntryID(track_desc.track_id) );
}

PlaylistID AIMPManager30::getPlayingPlaylist() const
{
    return cast<PlaylistID>( aimp3_playlist_manager_->StoragePlayingGet() );
}

PlaylistEntryID AIMPManager30::getPlayingEntry() const
{
    using namespace AIMP3SDK;
    const PlaylistID playing_playlist = getPlayingPlaylist();

    if (playing_playlist == 0) {
        // player is stopped.
        return 0;
    }

    const AIMP3SDK::HPLS playlist_handle = cast<AIMP3SDK::HPLS>(playing_playlist);

    int internal_playing_entry_index;
    HRESULT r = aimp3_playlist_manager_->StoragePropertyGetValue( playlist_handle, AIMP_PLAYLIST_STORAGE_PROPERTY_PLAYINGINDEX,
                                                                  &internal_playing_entry_index, sizeof(internal_playing_entry_index) 
                                                                 );
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_PLAYINGINDEX) in "__FUNCTION__);
    }

    const HPLSENTRY entry_handle = aimp3_playlist_manager_->StorageGetEntry(playlist_handle, internal_playing_entry_index);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in StorageGetEntry in "__FUNCTION__);
    }

    return castToPlaylistEntryID(entry_handle);
}

TrackDescription AIMPManager30::getPlayingTrack() const
{
    return TrackDescription( getPlayingPlaylist(), getPlayingEntry() );
}

AIMPManager::PLAYLIST_ENTRY_SOURCE_TYPE AIMPManager30::getTrackSourceType(TrackDescription track_desc) const // throws std::runtime_error
{
    const DWORD duration = getEntryField<DWORD>(playlists_db_, "duration", getAbsoluteEntryID(track_desc.track_id));
    return duration == 0 ? SOURCE_TYPE_RADIO : SOURCE_TYPE_FILE; // very shallow determination. Duration can be 0 on usual track if AIMP has no loaded track info yet.
}

AIMPManager30::PLAYBACK_STATE AIMPManager30::getPlaybackState() const
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

void AIMPManager30::enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) // throws std::runtime_error
{
    using namespace AIMP3SDK;
    AIMP3SDK::HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));

    HRESULT r = aimp3_playlist_manager_->QueueEntryAdd(entry_handle, insert_at_queue_beginning);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager30::removeEntryFromPlayQueue(TrackDescription track_desc) // throws std::runtime_error
{
    using namespace AIMP3SDK;
    AIMP3SDK::HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));

    HRESULT r = aimp3_playlist_manager_->QueueEntryRemove(entry_handle);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager30::saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width, int cover_height) const // throw std::runtime_error
{
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

std::auto_ptr<ImageUtils::AIMPCoverImage> AIMPManager30::getCoverImage(TrackDescription track_desc, int cover_width, int cover_height) const
{
    if (cover_width < 0 || cover_height < 0) {
        throw std::invalid_argument(MakeString() << "Error in "__FUNCTION__ << ". Negative cover size.");
    }

    const std::wstring& entry_filename = getEntryField<std::wstring>(playlists_db_, "filename", getAbsoluteEntryID(track_desc.track_id));
    HBITMAP cover_bitmap_handle = aimp3_coverart_manager_->CoverArtGetForFile(const_cast<PWCHAR>( entry_filename.c_str() ), nullptr,
			                                                                  nullptr, 0);

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

        cover_bitmap_handle = aimp3_coverart_manager_->CoverArtGetForFile(const_cast<PWCHAR>( entry_filename.c_str() ), &cover_size,
			                                                              nullptr, 0);
    }

    using namespace ImageUtils;
    return std::auto_ptr<AIMPCoverImage>( new AIMPCoverImage(cover_bitmap_handle) ); // do not close handle of AIMP bitmap.
}

bool AIMPManager30::isCoverImageFileExist(TrackDescription track_desc, boost::filesystem::wpath* path) const // throw std::runtime_error
{
    const std::wstring& entry_filename = getEntryField<std::wstring>(playlists_db_, "filename", getAbsoluteEntryID(track_desc.track_id));

    WCHAR coverart_filename_buffer[MAX_PATH + 1] = {0};
    aimp3_coverart_manager_->CoverArtGetForFile(const_cast<PWCHAR>( entry_filename.c_str() ), NULL,
	                                            coverart_filename_buffer, MAX_PATH);
    boost::system::error_code ignored_ec;
    const bool exists = fs::exists(coverart_filename_buffer, ignored_ec);
    
    if (exists && path) {
        *path = coverart_filename_buffer;
    }
    
    return exists;
}

void getEntryField_(sqlite3* db, const char* field, PlaylistEntryID entry_id, std::function<void(sqlite3_stmt*)> row_callback)
{
    using namespace Utilities;

    std::ostringstream query;

    query << "SELECT " << field
          << " FROM PlaylistsEntries WHERE entry_id=" << entry_id;

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
                                                 << ". Query: " << query;
            throw std::runtime_error(msg);
		}
    }

    throw std::runtime_error(MakeString() << "Error in "__FUNCTION__ << ". Entry " << entry_id << " does not exist");
}

template<>
std::wstring getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id)
{
    std::wstring r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_TEXT);
        if (sqlite3_column_type(stmt, 0) != SQLITE_TEXT) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Entry " << entry_id);
        }
        r = static_cast<const std::wstring::value_type*>(sqlite3_column_text16(stmt, 0));
    };
    getEntryField_(db, field, entry_id, handler);
    return r;
}

template<>
DWORD getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id)
{
    DWORD r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
        if (sqlite3_column_type(stmt, 0) != SQLITE_INTEGER) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Entry " << entry_id);
        }
        r = static_cast<DWORD>(sqlite3_column_int(stmt, 0));
    };
    getEntryField_(db, field, entry_id, handler);
    return r;
}

template<>
INT64 getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id)
{
    INT64 r;
    auto handler = [&](sqlite3_stmt* stmt) {
        assert(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
        if (sqlite3_column_type(stmt, 0) != SQLITE_INTEGER) {
            throw std::runtime_error(MakeString() << "Unexpected column type at "__FUNCTION__ << ": " << sqlite3_column_type(stmt, 0) << ". Entry " << entry_id);
        }
        r = sqlite3_column_int64(stmt, 0);
    };
    getEntryField_(db, field, entry_id, handler);
    return r;
}

void AIMPManager30::notifyAllExternalListeners(EVENTS event) const
{
    BOOST_FOREACH(const auto& listener_pair, external_listeners_) {
        const EventsListener& listener = listener_pair.second;
        listener(event);
    }
}

AIMPManager30::EventsListenerID AIMPManager30::registerListener(AIMPManager30::EventsListener listener)
{
    external_listeners_[next_listener_id_] = listener;
    assert(next_listener_id_ != UINT32_MAX);
    return ++next_listener_id_; // get next unique listener ID using simple increment.
}

void AIMPManager30::unRegisterListener(AIMPManager30::EventsListenerID listener_id)
{
    external_listeners_.erase(listener_id);
}

std::wstring AIMPManager30::getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const // throw std::invalid_argument
{
    std::wstring wformat_string( StringEncoding::utf8_to_utf16(format_string_utf8) );

    { // use AIMP3 format specifiers.
    using namespace Utilities;
    // Aimp3 uses %R instead %a as Artist.
    replaceAll(L"%a", 2,
               L"%R", 2,
               &wformat_string);
    }

    using namespace AIMP3SDK;
    const HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));

    AIMP3Util::FileInfoHelper file_info_helper;
    HRESULT r = aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_INFO,
                                                                &file_info_helper.getEmptyFileInfo(), sizeof(file_info_helper.getEmptyFileInfo()) ); 
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error of IAIMPAddonsPlaylistManager::EntryPropertyGetValue() " << r << " in "__FUNCTION__" with " << format_string_utf8);
    }
    
    PWCHAR formatted_string = nullptr;
    r = aimp3_playlist_manager_->FormatString(  const_cast<PWCHAR>( wformat_string.c_str() ),
                                                wformat_string.length(),
                                                AIMP_PLAYLIST_FORMAT_MODE_FILEINFO, // since AIMP_PLAYLIST_FORMAT_MODE_PREVIEW expands %R as "Artist" and %T as "Title" we use AIMP_PLAYLIST_FORMAT_MODE_FILEINFO which works as expected.
                                                &file_info_helper.getFileInfo(),
                                                &formatted_string
                                              );
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error of IAIMPAddonsPlaylistManager::FormatString() " << r << " in "__FUNCTION__" with " << format_string_utf8);
    }

    return formatted_string;
}

void AIMPManager30::trackRating(TrackDescription track_desc, int rating) // throws std::runtime_error
{
    using namespace AIMP3SDK;
    HPLSENTRY entry_handle = castToHPLSENTRY(getAbsoluteEntryID(track_desc.track_id));
    HRESULT r = aimp3_playlist_manager_->EntryPropertySetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_MARK, &rating, sizeof(rating) );    
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << "Error " << r << " in "__FUNCTION__", track " << track_desc);
    }  
    // Note: at this point entry does not exist any more, since EntryPropertySetValue forces calling of onStorageChanged() so, entries are reloaded.
}

int AIMPManager30::trackRating(TrackDescription track_desc) const // throws std::runtime_error
{
    return getEntryField<DWORD>(playlists_db_, "rating", getAbsoluteEntryID(track_desc.track_id));
}

void AIMPManager30::initPlaylistDB() // throws std::runtime_error
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
                                                      "PRIMARY KEY (entry_id)"
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

void AIMPManager30::shutdownPlaylistDB()
{
    const int rc = sqlite3_close(playlists_db_);
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << "sqlite3_close error: " << rc;
    }
    playlists_db_ = nullptr;
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

void AIMPManager30::deletePlaylistFromPlaylistDB(PlaylistID playlist_id)
{
    deletePlaylistEntriesFromPlaylistDB(playlist_id);

    const std::string query = MakeString() << "DELETE FROM Playlists WHERE id=" << playlist_id;

    executeQuery(query, playlists_db_, __FUNCTION__);
}

void AIMPManager30::deletePlaylistEntriesFromPlaylistDB(PlaylistID playlist_id)
{
    const std::string query = MakeString() << "DELETE FROM PlaylistsEntries WHERE playlist_id=" << playlist_id;

    executeQuery(query, playlists_db_, __FUNCTION__);
}

} // namespace AIMPPlayer
