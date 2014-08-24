// Copyright (c) 2014, Alexey Ivanov

#include "stdafx.h"
#include "manager3.6.h"
#include "plugin/logger.h"
#include "utils/sqlite_util.h"
#include "sqlite/sqlite.h"
#include "utils/iunknown_impl.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}

namespace AIMPPlayer
{

using namespace Utilities;
using namespace AIMP36SDK;

class AIMPExtensionPlaylistManagerListener : public IUnknownInterfaceImpl<AIMP36SDK::IAIMPExtensionPlaylistManagerListener>
{
public:
    explicit AIMPExtensionPlaylistManagerListener(AIMPManager36* aimp36_manager)
        : 
        aimp36_manager_(aimp36_manager)
    {}

    virtual void WINAPI PlaylistActivated(IAIMPPlaylist* playlist) {
        aimp36_manager_->playlistActivated(playlist);
    }
	virtual void WINAPI PlaylistAdded(IAIMPPlaylist* playlist) {
        aimp36_manager_->playlistAdded(playlist);
    }
	virtual void WINAPI PlaylistRemoved(IAIMPPlaylist* playlist) {
        aimp36_manager_->playlistRemoved(playlist);
    }

    virtual HRESULT WINAPI QueryInterface(REFIID riid, LPVOID* ppvObj) {
        if (!ppvObj) {
            return E_POINTER;
        }

        if (IID_IUnknown == riid) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        } else if (AIMP36SDK::IID_IAIMPExtensionPlaylistManagerListener == riid) {
            *ppvObj = this;
            AddRef();
            return S_OK;                
        }

        return E_NOINTERFACE;
    }

private:

    AIMPManager36* aimp36_manager_;
};

AIMPManager36::AIMPManager36(boost::intrusive_ptr<AIMP36SDK::IAIMPCore> aimp36_core)
    :   playlists_db_(nullptr),
        aimp36_core_(aimp36_core)
{
    try {
        initializeAIMPObjects();

        initPlaylistDB();

        // register listeners here
        HRESULT r = aimp36_core->RegisterExtension(IID_IAIMPServicePlaylistManager, new AIMPExtensionPlaylistManagerListener(this));
        if (S_OK != r) {
            throw std::runtime_error("RegisterExtension(IID_IAIMPServicePlaylistManager) failed"); 
        }
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured during AIMPManager36 initialization. Reason:") + e.what() );
    }
}

AIMPManager36::~AIMPManager36()
{
    // It seems listeners will be released by AIMP before Finalize call.

    aimp_service_playlist_manager_.reset();

    shutdownPlaylistDB();
}

void AIMPManager36::initializeAIMPObjects()
{
    IAIMPServicePlaylistManager* playlist_manager;
    if (S_OK != aimp36_core_->QueryInterface(IID_IAIMPServicePlaylistManager,
                                             reinterpret_cast<void**>(&playlist_manager)
                                             ) 
        )
    {
        throw std::runtime_error("Creation object IAIMPServicePlaylistManager failed"); 
    }
    aimp_service_playlist_manager_.reset(playlist_manager);
    playlist_manager->Release();

    /*
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
    */
}

void AIMPManager36::initPlaylistDB()
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
                                                      "entry_index    INTEGER,"
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

void AIMPManager36::shutdownPlaylistDB()
{
    const int rc = sqlite3_close(playlists_db_);
    if (SQLITE_OK != rc) {
        BOOST_LOG_SEV(logger(), error) << "sqlite3_close error: " << rc;
    }
    playlists_db_ = nullptr;
}

void AIMPManager36::playlistActivated(AIMP36SDK::IAIMPPlaylist* /*playlist*/)
{
    BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::playlistActivated"; ///!!! TODO: implement
}

template<>
PlaylistID cast(IAIMPPlaylist* playlist)
{
    return reinterpret_cast<PlaylistID>(playlist);
}

template<>
IAIMPPlaylist* cast(PlaylistID id)
{
    return reinterpret_cast<IAIMPPlaylist*>(id);
}

void AIMPManager36::playlistAdded(IAIMPPlaylist* playlist)
{
    try {
        BOOST_LOG_SEV(logger(), debug) << "onStorageAdded: id = " << cast<PlaylistID>(playlist);
        int playlist_index = getPlaylistIndexByHandle(playlist);
        playlist_index = playlist_index;
        loadPlaylist(playlist, playlist_index);
        notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__ << " for playlist with handle " << cast<PlaylistID>(playlist) << ". Reason: " << e.what();
    } catch (...) {
        // we can't propagate exception from here since it is called from AIMP. Just log unknown error.
        BOOST_LOG_SEV(logger(), error) << "Unknown exception in "__FUNCTION__ << " for playlist with handle " << cast<PlaylistID>(playlist);
    }
}

void AIMPManager36::loadPlaylist(IAIMPPlaylist* playlist, int playlist_index)
{
    const PlaylistID playlist_id = cast<PlaylistID>(playlist);

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
    
    
    IAIMPPropertyList* playlist_propertylist_tmp;
    HRESULT r = playlist->QueryInterface(IID_IAIMPPropertyList,
                                         reinterpret_cast<void**>(&playlist_propertylist_tmp));
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "playlist->QueryInterface(IID_IAIMPPropertyList) failed. Result " << r);
    }
    boost::intrusive_ptr<IAIMPPropertyList> playlist_propertylist(playlist_propertylist_tmp, false);
    playlist_propertylist_tmp = nullptr;


    INT64 duration;
    
    r = playlist_propertylist->GetValueAsInt64(AIMP_PLAYLIST_PROPID_DURATION, &duration);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPPropertyList::GetValueAsInt64(AIMP_PLAYLIST_PROPID_DURATION) failed. Result " << r);
    }

    INT64 size = 0; ///!!! it seems it does not supported in 3.60.
    /*
    r = playlist_propertylist->GetValueAsInt64(AIMP_PLAYLIST_PROPID_SIZE, &size);
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPPropertyList::GetValueAsInt64(AIMP_PLAYLIST_PROPID_SIZE) failed. Result " << r);
    }
    */

    IAIMPString* name_tmp;
    r = playlist_propertylist->GetValueAsObject(AIMP_PLAYLIST_PROPID_NAME, IID_IAIMPString, reinterpret_cast<void**>(&name_tmp));
    if (S_OK != r) {
        throw std::runtime_error(MakeString() << error_prefix << "IAIMPAddonsPlaylistManager::StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_NAME) failed. Result " << r);
    }
    boost::intrusive_ptr<IAIMPString> nameStr(name_tmp, false);
    const WCHAR* name = nameStr->GetData();

    const int entries_count = playlist->GetItemCount();

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
    bind(int,   1, playlist_id);
    bind(int,   2, playlist_index);
    bindText(   3, name, nameStr->GetLength());
    bind(int,   4, entries_count);
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

int AIMPManager36::getPlaylistIndexByHandle(IAIMPPlaylist* playlist)
{
    for (int i = 0, count = aimp_service_playlist_manager_->GetLoadedPlaylistCount(); i != count; ++i) {
        IAIMPPlaylist* current_playlist;
        HRESULT r = aimp_service_playlist_manager_->GetLoadedPlaylist(i, &current_playlist);
        if (S_OK != r) {
            throw std::runtime_error(MakeString() << "GetLoadedPlaylist failure: " << r);
        }
        current_playlist->Release();
        
        if (playlist == current_playlist) {
            return i;
        }
    }

    return -1;
}

void AIMPManager36::notifyAllExternalListeners(AIMPManager::EVENTS event) const
{
    for (const auto& listener_pair : external_listeners_) {
        const EventsListener& listener = listener_pair.second;
        listener(event);
    }
}

AIMPManager::EventsListenerID AIMPManager36::registerListener(AIMPManager::EventsListener listener)
{
    external_listeners_[next_listener_id_] = listener;
    assert(next_listener_id_ != UINT32_MAX);
    return ++next_listener_id_; // get next unique listener ID using simple increment.
}

void AIMPManager36::unRegisterListener(AIMPManager::EventsListenerID listener_id)
{
    external_listeners_.erase(listener_id);
}

void AIMPManager36::playlistRemoved(AIMP36SDK::IAIMPPlaylist* /*playlist*/)
{
    BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::playlistRemoved"; ///!!! TODO: implement
}

void AIMPManager36::startPlayback()
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::startPlayback"; ///!!! TODO: implement
}

void AIMPManager36::startPlayback(TrackDescription /*track_desc*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::startPlayback"; ///!!! TODO: implement
}

void AIMPManager36::stopPlayback()
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::stopPlayback"; ///!!! TODO: implement
}

std::string AIMPManager36::getAIMPVersion() const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getAIMPVersion"; ///!!! TODO: implement
    return std::string();
}

void AIMPManager36::pausePlayback()
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::pausePlayback"; ///!!! TODO: implement
}

void AIMPManager36::playNextTrack()
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::playNextTrack"; ///!!! TODO: implement
}

void AIMPManager36::playPreviousTrack()
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::playPreviousTrack"; ///!!! TODO: implement
}

AIMPManager::StatusValue AIMPManager36::getStatus(STATUS /*status*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getStatus"; ///!!! TODO: implement
    return 0;
}

void AIMPManager36::setStatus(STATUS /*status*/, StatusValue /*value*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::setStatus"; ///!!! TODO: implement
}

void AIMPManager36::enqueueEntryForPlay(TrackDescription /*track_desc*/, bool /*insert_at_queue_beginning*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::enqueueEntryForPlay"; ///!!! TODO: implement
}

void AIMPManager36::removeEntryFromPlayQueue(TrackDescription /*track_desc*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::removeEntryFromPlayQueue"; ///!!! TODO: implement
}

PlaylistID AIMPManager36::getPlayingPlaylist() const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getPlayingPlaylist"; ///!!! TODO: implement
    return PlaylistID();
}

PlaylistEntryID AIMPManager36::getPlayingEntry() const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getPlayingEntry"; ///!!! TODO: implement
    return PlaylistEntryID();
}

TrackDescription AIMPManager36::getPlayingTrack() const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getPlayingTrack"; ///!!! TODO: implement
    return TrackDescription(0,0);
}

PlaylistID AIMPManager36::getAbsolutePlaylistID(PlaylistID /*id*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getAbsolutePlaylistID"; ///!!! TODO: implement
    return PlaylistID();
}

PlaylistEntryID AIMPManager36::getAbsoluteEntryID(PlaylistEntryID /*id*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getAbsoluteEntryID"; ///!!! TODO: implement
    return PlaylistEntryID();
}

TrackDescription AIMPManager36::getAbsoluteTrackDesc(TrackDescription /*track_desc*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getAbsoluteTrackDesc"; ///!!! TODO: implement
    return TrackDescription(0,0);
}

crc32_t AIMPManager36::getPlaylistCRC32(PlaylistID /*playlist_id*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getPlaylistCRC32"; ///!!! TODO: implement
    return crc32_t();
}

AIMPManager::PLAYLIST_ENTRY_SOURCE_TYPE AIMPManager36::getTrackSourceType(TrackDescription /*track_desc*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getTrackSourceType"; ///!!! TODO: implement
    return AIMPManager::SOURCE_TYPE_FILE;
}

AIMPManager::PLAYBACK_STATE AIMPManager36::getPlaybackState() const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getPlaybackState"; ///!!! TODO: implement
    return AIMPManager::STOPPED;
}

std::wstring AIMPManager36::getFormattedEntryTitle(TrackDescription /*track_desc*/, const std::string& /*format_string_utf8*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getFormattedEntryTitle"; ///!!! TODO: implement
    return std::wstring();
}

std::wstring AIMPManager36::getEntryFilename(TrackDescription /*track_desc*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::getEntryFilename"; ///!!! TODO: implement
    return std::wstring();
}

bool AIMPManager36::isCoverImageFileExist(TrackDescription /*track_desc*/, boost::filesystem::wpath* /*path*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::isCoverImageFileExist"; ///!!! TODO: implement
    return false;
}

void AIMPManager36::saveCoverToFile(TrackDescription /*track_desc*/, const std::wstring& /*filename*/, int /*cover_width*/, int /*cover_height*/ ) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::saveCoverToFile"; ///!!! TODO: implement
}

int AIMPManager36::trackRating(TrackDescription /*track_desc*/) const
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::trackRating"; ///!!! TODO: implement
    return 0;
}

void AIMPManager36::addFileToPlaylist(const boost::filesystem::wpath& /*path*/, PlaylistID /*playlist_id*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::addFileToPlaylist"; ///!!! TODO: implement
}
    
void AIMPManager36::addURLToPlaylist(const std::string& /*url*/, PlaylistID /*playlist_id*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::addURLToPlaylist"; ///!!! TODO: implement
}

PlaylistID AIMPManager36::createPlaylist(const std::wstring& /*title*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::createPlaylist"; ///!!! TODO: implement
    return PlaylistID();
}

void AIMPManager36::removeTrack(TrackDescription /*track_desc*/, bool /*physically*/)
{
	BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::removeTrack"; ///!!! TODO: implement
}

void AIMPManager36::onTick()
{
    //BOOST_LOG_SEV(logger(), debug) << "AIMPManager36::onTick"; ///!!! TODO: implement
}

} // namespace AIMPPlayer
