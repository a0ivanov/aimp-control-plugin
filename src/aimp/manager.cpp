// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "manager.h"
#include "playlist_entry.h"
#include "aimp2_sdk.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/image.h"
#include "utils/util.h"
#include <boost/assign/std.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <algorithm>

namespace {
using namespace AIMPControlPlugin::PluginLogger;
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

void IReleaseFunctor::operator()(IUnknown* object) const
{
    /* do nothing here
        Reason: tests shows that AIMP does not increment reference count of his objects before give them to plugin
        through IAIMP2Controller::AIMP_QueryObject().
        And the object IAIMP2Controller has zero reference count at plugin IAIMPAddonHeader::Initialize() call.
        Uncomment following code if reference count is properly handled by AIMP:
    */
    //#if _DEBUG
    //    ULONG ref_count = object->Release();
    //    BOOST_LOG_SEV(logger(), debug) << "AIMP Object " << reinterpret_cast<ULONG>(object) << " release() = " << ref_count;
    //#else
    //    object->Release();
    //#endif
}

using namespace AIMP2SDK;

AIMPManager::AIMPManager(boost::shared_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                         boost::asio::io_service& io_service_
                         )
    :
    aimp_controller_(aimp_controller),
    strand_(io_service_),
#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
    playlists_check_timer_( io_service_, boost::posix_time::seconds(kPLAYLISTS_CHECK_PERIOD_SEC) ),
#endif
    next_listener_id_(0)
{
    try {
        initializeAIMPObjects();
#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
        // set timer which will periodically check when AIMP has load playlists.
        playlists_check_timer_.async_wait( boost::bind(&AIMPManager::onTimerPlaylistsChangeCheck, this, _1) );
#endif
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured while AIMPManager initialization. Reason:") + e.what() );
    }

    registerNotifiers();
}

AIMPManager::~AIMPManager()
{
    unregisterNotifiers();

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION
    BOOST_LOG_SEV(logger(), info) << "Stopping playlists check timer.";
    playlists_check_timer_.cancel();
    BOOST_LOG_SEV(logger(), info) << "Playlists check timer was stopped.";
#endif
}

Playlist AIMPManager::loadPlaylist(int playlist_index)
{
    const char * const error_prefix = "Error occured while extracting playlist data: ";
    
    int id;
    if ( S_OK != aimp_playlist_manager_->AIMP_PLS_ID_By_Index(playlist_index, &id) ) {
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_ID_By_Index failed");
    }

    INT64 duration, size;
    if ( S_OK != aimp_playlist_manager_->AIMP_PLS_GetInfo(id, &duration, &size) ) {
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_GetInfo failed");
    }

    const size_t name_length = 256;
    WCHAR name[name_length + 1] = {0};
    if ( S_OK != aimp_playlist_manager_->AIMP_PLS_GetName(id, name, name_length) ) {    
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_GetName failed");
    }

    const int files_count = aimp_playlist_manager_->AIMP_PLS_GetFilesCount(id);

    return Playlist(name,
                    files_count,
                    duration,
                    size,
                    id
                    );
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
        = kFIELDBUFFERSIZE;
        // set buffers
        info_.sAlbum = album;
        info_.sArtist = artist;
        info_.sDate = date;
        info_.sFileName = filename;
        info_.sGenre = genre;
        info_.sTitle = title;

        return info_;
    }

    AIMP2SDK::AIMP2FileInfo& getFileInfo()
        { return info_; }

    PlaylistEntry getPlaylistEntry(DWORD entry_id, crc32_t crc32 = 0)
    {
        getFileInfoWithCorrectStringLengths();

        return PlaylistEntry(   info_.sAlbum,    info_.nAlbumLen,
                                info_.sArtist,   info_.nArtistLen,
                                info_.sDate,     info_.nDateLen,
                                info_.sFileName, info_.nFileNameLen,
                                info_.sGenre,    info_.nGenreLen,
                                info_.sTitle,    info_.nTitleLen,
                                // info_.nActive - useless, not used.
                                info_.nBitRate,
                                info_.nChannels,
                                info_.nDuration,
                                info_.nFileSize,
                                info_.nRating,
                                info_.nSampleRate,
                                info_.nTrackID,
                                entry_id,
                                crc32
                            );
    }

    /* \return track ID. */
    DWORD getTrackID() const
        { return info_.nTrackID; }

    AIMP2SDK::AIMP2FileInfo& getFileInfoWithCorrectStringLengths()
    {
        // fill string lengths if Aimp do not do this.
        if (info_.nAlbumLen == kFIELDBUFFERSIZE) {
            info_.nAlbumLen = std::wcslen(info_.sAlbum);
        }

        if (info_.nArtistLen == kFIELDBUFFERSIZE) {
            info_.nArtistLen = std::wcslen(info_.sArtist);
        }

        if (info_.nDateLen == kFIELDBUFFERSIZE) {
            info_.nDateLen = std::wcslen(info_.sDate);
        }

        if (info_.nFileNameLen == kFIELDBUFFERSIZE) {
            info_.nFileNameLen = std::wcslen(info_.sFileName);
        }

        if (info_.nGenreLen == kFIELDBUFFERSIZE) {
            info_.nGenreLen = std::wcslen(info_.sGenre);
        }

        if (info_.nTitleLen == kFIELDBUFFERSIZE) {
            info_.nTitleLen = std::wcslen(info_.sTitle);
        }

        return info_;
    }

private:

    AIMP2SDK::AIMP2FileInfo info_;
    static const DWORD kFIELDBUFFERSIZE = 256;
    WCHAR album[kFIELDBUFFERSIZE];
    WCHAR artist[kFIELDBUFFERSIZE];
    WCHAR date[kFIELDBUFFERSIZE];
    WCHAR filename[kFIELDBUFFERSIZE];
    WCHAR genre[kFIELDBUFFERSIZE];
    WCHAR title[kFIELDBUFFERSIZE];
};

void AIMPManager::loadEntries(Playlist& playlist) // throws std::runtime_error
{
    // PROFILE_EXECUTION_TIME(__FUNCTION__);
    const PlaylistID playlist_id = playlist.getID();
    const int entries_count = aimp_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);

    AIMP2FileInfoHelper file_info_helper; // used for get entries from AIMP conviniently.

    // temp objects to prevent partial change state of passed objects when error occurs.
    EntriesListType entries;
    entries.reserve(entries_count);
    
    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        // aimp_playlist_manager_->AIMP_PLS_Entry_ReloadInfo(id_, entry_index); // try to make AIMP update track info: this takes significant time and some tracks are not updated anyway.
        if ( aimp_playlist_manager_->AIMP_PLS_Entry_InfoGet(playlist_id,
                                                            entry_index,
                                                            &file_info_helper.getEmptyFileInfo()
                                                            )
            )
        {
            entries.push_back( file_info_helper.getPlaylistEntry(entry_index) );
        } else {
            BOOST_LOG_SEV(logger(), error) << "Error occured while getting entry info ¹" << entry_index << " from playlist with ID = " << playlist_id;
            throw std::runtime_error("Error occured while getting playlist entries.");
        }
    }

    // we got list, save result
    playlist.getEntries().swap(entries);
}

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

void AIMPManager::checkIfPlaylistsChanged()
{
    std::vector<PlaylistID> playlists_to_reload;

    const short playlists_count = aimp_playlist_manager_->AIMP_PLS_Count();
    for (short playlist_index = 0; playlist_index < playlists_count; ++playlist_index) {
        int current_playlist_id;
        if ( S_OK != aimp_playlist_manager_->AIMP_PLS_ID_By_Index(playlist_index, &current_playlist_id) ) {
            throw std::runtime_error(MakeString() << "checkIfPlaylistsChanged() failed. Reason: AIMP_PLS_ID_By_Index failed");
        }

        const int entries_count = aimp_playlist_manager_->AIMP_PLS_GetFilesCount(current_playlist_id);

        const auto loaded_playlist_iter = playlists_.find(current_playlist_id);
        if (playlists_.end() == loaded_playlist_iter) {
            // current playlist was not loaded yet, load it now without entries.
            playlists_.insert( std::make_pair(current_playlist_id, loadPlaylist(playlist_index) ) );
            playlists_to_reload.push_back(current_playlist_id);
        } else if (loaded_playlist_iter->second.getEntriesCount() != entries_count) {
            // list loaded, but entries count is changed: load new playlist to update all internal fields.
            playlists_[current_playlist_id] = loadPlaylist(playlist_index); // we must use map's operator[] instead insert() method, since insert is no-op for existing key.
            playlists_to_reload.push_back(current_playlist_id);
        } else if ( !isLoadedPlaylistEqualsAimpPlaylist(current_playlist_id) ) {
            // contents of loaded playlist and current playlist are different.
            playlists_to_reload.push_back(current_playlist_id);
        }
    }

    // reload entries of playlists from list.
    BOOST_FOREACH(PlaylistID playlist_id, playlists_to_reload) {
        try {
            auto it = playlists_.find(playlist_id);
            assert( it != playlists_.end() );
            Playlist& playlist = it->second;
            loadEntries(playlist); // avoid using of playlists_[playlist_id], since it requires Playlist's default ctor.
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error occured while playlist(id = " << playlist_id << ") entries loading. Reason: " << e.what();
        }
    }

    if ( !playlists_to_reload.empty() ) {
        notifyAboutInternalEvent(PLAYLISTS_CONTENT_CHANGED_EVENT);
    }
}

bool AIMPManager::isLoadedPlaylistEqualsAimpPlaylist(PlaylistID playlist_id) const
{
    // PROFILE_EXECUTION_TIME(__FUNCTION__);

    const Playlist& playlist = getPlaylist(playlist_id);
    const EntriesListType& loaded_entries = playlist.getEntries();

    const int entries_count = aimp_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);
    assert( entries_count >= 0 && static_cast<size_t>(entries_count) == loaded_entries.size() ); // function returns correct result only if entries count in loaded and actual playlists are equal.

    AIMP2FileInfoHelper file_info_helper; // used for get entries from AIMP conviniently.

    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        if ( aimp_playlist_manager_->AIMP_PLS_Entry_InfoGet( playlist_id,
                                                             entry_index,
                                                             &file_info_helper.getEmptyFileInfo()
                                                           )
           )
        {
            // need to compare loaded_entry with file_info_helper.info_;
            const AIMP2SDK::AIMP2FileInfo& aimp_entry = file_info_helper.getFileInfoWithCorrectStringLengths();
            if ( loaded_entries[entry_index].getCRC32() != Utilities::crc32(aimp_entry) ) {
                return false;
            }
        } else {
            // in case of Aimp error just say that playlists are not equal.
            return false;
        }
    }

    return true;
}

void AIMPManager::onTimerPlaylistsChangeCheck(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        // Restart timer.
        playlists_check_timer_.expires_from_now( boost::posix_time::seconds(kPLAYLISTS_CHECK_PERIOD_SEC) );
        playlists_check_timer_.async_wait( boost::bind( &AIMPManager::onTimerPlaylistsChangeCheck, this, _1 ) );

        // Do check
        checkIfPlaylistsChanged();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

#endif // #ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

void AIMPManager::registerCallbackRange(int id_first, int id_last)
{
    for (int callback_id = id_first; callback_id != id_last; ++callback_id) {
        // register notifier.
        boolean result = aimp_controller_->AIMP_CallBack_Set( callback_id,
                                                             &internalAIMPStateNotifier,
                                                             reinterpret_cast<DWORD>(this) // user data that will be passed in internalAIMPStateNotifier().
                                                            );
        if (!result) {
            BOOST_LOG_SEV(logger(), error) << "Error occured while register " << callback_id << " callback";
        }
    }
}

void AIMPManager::registerNotifiers()
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
        boolean result = aimp_controller_->AIMP_CallBack_Set( callback_id,
                                                              &internalAIMPStateNotifier,
                                                              reinterpret_cast<DWORD>(this) // user data that will be passed in internalAIMPStateNotifier().
                                                             );
        if (!result) {
            BOOST_LOG_SEV(logger(), error) << "Error occured while register " << callback.second << " callback";
        }
    }

    // ID of playlists is sent when playlist is activated or changed.
    //registerCallbackRange(AIMP_INFO_UPDATE + 1, AIMP_PLAYER_STATE);
    //registerCallbackRange(AIMP_TRACK_POS_CHANGED + 1, 100);
}

void AIMPManager::unregisterNotifiers()
{
    BOOST_FOREACH(const auto& callback, aimp_callback_names_) {
        const int callback_id = callback.first;
        // unregister notifier.
        boolean result = aimp_controller_->AIMP_CallBack_Remove(callback_id, &internalAIMPStateNotifier);
        if (!result) {
            BOOST_LOG_SEV(logger(), error) <<  "Error occured while unregister " << callback.second << " callback";
        }
    }
}

void AIMPManager::initializeAIMPObjects()
{
    // get IAIMP2Player object.
    IAIMP2Player* player = nullptr;
    boolean result = aimp_controller_->AIMP_QueryObject(IAIMP2PlayerID, &player);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2Player failed");
    }
    aimp_player_.reset( player, IReleaseFunctor() );

    // get IAIMP2PlaylistManager2 object.
    IAIMP2PlaylistManager2* playlist_manager = nullptr;
    result = aimp_controller_->AIMP_QueryObject(IAIMP2PlaylistManager2ID, &playlist_manager);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2PlaylistManager2 failed");
    }
    aimp_playlist_manager_.reset(playlist_manager, IReleaseFunctor());

    // get IAIMP2Extended object.
    IAIMP2Extended* extended = nullptr;
    result = aimp_controller_->AIMP_QueryObject(IAIMP2ExtendedID, &extended);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2Extended failed");
    }
    aimp_extended_.reset( extended, IReleaseFunctor() );

    // get IAIMP2CoverArtManager object.
    IAIMP2CoverArtManager* cover_art_manager = nullptr;
    result = aimp_controller_->AIMP_QueryObject(IAIMP2CoverArtManagerID, &cover_art_manager);
    if (!result) {
        throw std::runtime_error("Creation object IAIMP2CoverArtManager failed");
    }
    aimp_cover_art_manager_.reset( cover_art_manager, IReleaseFunctor() );
}

void AIMPManager::startPlayback()
{
    // play current track.
    aimp_player_->PlayOrResume();
}

void AIMPManager::startPlayback(TrackDescription track_desc) // throws std::runtime_error
{
    if ( FALSE == aimp_player_->PlayTrack(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__" with " << track_desc );
    }
}

void AIMPManager::stopPlayback()
{
    aimp_player_->Stop();
}

void AIMPManager::pausePlayback()
{
    aimp_player_->Pause();
}

void AIMPManager::playNextTrack()
{
    aimp_player_->NextTrack();
}

void AIMPManager::playPreviousTrack()
{
    aimp_player_->PrevTrack();
}

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);
typedef int AIMP2SDK_STATUS;

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
    case AIMPManager::STATUS_ON_STOP:   return AIMP_STS_ON_STOP;
    case AIMPManager::STATUS_POS:       return AIMP_STS_POS;
    case AIMPManager::STATUS_LENGTH:    return AIMP_STS_LENGTH;
    case AIMPManager::STATUS_REPEATPLS: return AIMP_STS_REPEATPLS;
    case AIMPManager::STATUS_REP_PLS_1: return AIMP_STS_REP_PLS_1;
    case AIMPManager::STATUS_KBPS:      return AIMP_STS_KBPS;
    case AIMPManager::STATUS_KHZ:       return AIMP_STS_KHZ;
    case AIMPManager::STATUS_MODE:      return AIMP_STS_MODE;
    case AIMPManager::STATUS_RADIO:     return AIMP_STS_RADIO;
    case AIMPManager::STATUS_STREAM_TYPE: return AIMP_STS_STREAM_TYPE;
    case AIMPManager::STATUS_TIMER:     return AIMP_STS_TIMER;
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
AIMPManager::STATUS cast(AIMP2SDK_STATUS status) // throws std::bad_cast
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
    case AIMP_STS_ON_STOP:   return AIMPManager::STATUS_ON_STOP;
    case AIMP_STS_POS:       return AIMPManager::STATUS_POS;
    case AIMP_STS_LENGTH:    return AIMPManager::STATUS_LENGTH;
    case AIMP_STS_REPEATPLS: return AIMPManager::STATUS_REPEATPLS;
    case AIMP_STS_REP_PLS_1: return AIMPManager::STATUS_REP_PLS_1;
    case AIMP_STS_KBPS:      return AIMPManager::STATUS_KBPS;
    case AIMP_STS_KHZ:       return AIMPManager::STATUS_KHZ;
    case AIMP_STS_MODE:      return AIMPManager::STATUS_MODE;
    case AIMP_STS_RADIO:     return AIMPManager::STATUS_RADIO;
    case AIMP_STS_STREAM_TYPE: return AIMPManager::STATUS_STREAM_TYPE;
    case AIMP_STS_TIMER:     return AIMPManager::STATUS_TIMER;
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

const char* asString(AIMPManager::STATUS status)
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
    case AIMPManager::STATUS_ON_STOP:   return "ON_STOP";
    case AIMPManager::STATUS_POS:       return "POS";
    case AIMPManager::STATUS_LENGTH:    return "LENGTH";
    case AIMPManager::STATUS_REPEATPLS: return "REPEATPLS";
    case AIMPManager::STATUS_REP_PLS_1: return "REP_PLS_1";
    case AIMPManager::STATUS_KBPS:      return "KBPS";
    case AIMPManager::STATUS_KHZ:       return "KHZ";
    case AIMPManager::STATUS_MODE:      return "MODE";
    case AIMPManager::STATUS_RADIO:     return "RADIO";
    case AIMPManager::STATUS_STREAM_TYPE: return "STREAM_TYPE";
    case AIMPManager::STATUS_TIMER:     return "TIMER";
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

void AIMPManager::notifyAboutInternalEventOnStatusChange(AIMPManager::STATUS status)
{
    switch (status) {
    case STATUS_SHUFFLE:
        notifyAboutInternalEvent(SHUFFLE_EVENT);
        break;
    case STATUS_REPEAT:
        notifyAboutInternalEvent(REPEAT_EVENT);
        break;
    case STATUS_VOLUME:
        notifyAboutInternalEvent(VOLUME_EVENT);
        break;
    case STATUS_MUTE:
        notifyAboutInternalEvent(MUTE_EVENT);
        break;
    case STATUS_POS:
        notifyAboutInternalEvent(TRACK_PROGRESS_CHANGED_DIRECTLY_EVENT);
        break;
    default:
        // do nothing, about other status changes AIMP will notify us itself.
        break;
    }
}

void AIMPManager::setStatus(AIMPManager::STATUS status, AIMPManager::StatusValue value)
{
    try {
        if ( FALSE == aimp_controller_->AIMP_Status_Set(cast<AIMP2SDK_STATUS>(status), value) ) {
            throw std::runtime_error(MakeString() << "Error occured while setting status " << asString(status) << " to value " << value);
        }
    } catch (std::bad_cast& e) {
        throw std::runtime_error( e.what() );
    }

    notifyAboutInternalEventOnStatusChange(status);
}

AIMPManager::StatusValue AIMPManager::getStatus(AIMPManager::STATUS status) const
{
    return aimp_controller_->AIMP_Status_Get(status);
}

std::string AIMPManager::getAIMPVersion() const
{
    const int version = aimp_controller_->AIMP_GetSystemVersion(); // IAIMP2Player::Version() is not used since it is always returns 1. Maybe it is Aimp SDK version?
    using namespace std;
    ostringstream os;
    os << version / 1000 << '.' << setfill('0') << setw(2) << (version % 1000) / 10 << '.' << version % 10;
    return os.str();
}

PlaylistID AIMPManager::getActivePlaylist() const
{
    // return AIMP internal playlist ID here since AIMPManager uses the same IDs.
    return aimp_playlist_manager_->AIMP_PLS_ID_PlayingGet();
}

PlaylistEntryID AIMPManager::getActiveEntry() const
{
    const PlaylistID active_playlist = getActivePlaylist();
    const int internal_active_entry_index = aimp_playlist_manager_->AIMP_PLS_ID_PlayingGetTrackIndex(active_playlist);
    // internal index equals AIMPManager ID. In other case map index<->ID(use Playlist::entries_id_list_) here in all places where TrackDescription is used.
    const PlaylistEntryID entry_id = internal_active_entry_index;
    return entry_id;
}

TrackDescription AIMPManager::getActiveTrack() const
{
    return TrackDescription( getActiveEntry(), getActivePlaylist() );
}

AIMPManager::PLAYBACK_STATE AIMPManager::getPlaybackState() const
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

void AIMPManager::enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) // throws std::runtime_error
{
    if ( S_OK != aimp_playlist_manager_->AIMP_PLS_Entry_QueueSet(track_desc.playlist_id, track_desc.track_id, insert_at_queue_beginning) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

void AIMPManager::removeEntryFromPlayQueue(TrackDescription track_desc) // throws std::runtime_error
{
    if ( S_OK != aimp_playlist_manager_->AIMP_PLS_Entry_QueueRemove(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

const AIMPManager::PlaylistsListType& AIMPManager::getPlayLists() const
{
    return playlists_;
}

void AIMPManager::savePNGCoverToVector(TrackDescription track_desc, int cover_width, int cover_height, std::vector<BYTE>& image_data) const
{
    std::vector<BYTE> image_data_temp; // will be contain BMP image data.
    try {
        using namespace ImageUtils;
        std::auto_ptr<AIMPCoverImage> cover( getCoverImage(track_desc, cover_width, cover_height) );
        cover->saveToVector(AIMPCoverImage::PNG_IMAGE, image_data_temp);
    } catch (std::exception& e) {
        const std::string& str = MakeString() << "Error occured while cover saving to vector for " << track_desc << ". Reason: " << e.what();
        BOOST_LOG_SEV(logger(), error) << str;
        throw std::runtime_error(str);
    }

    // we got image, save it now.
    image_data.swap(image_data_temp);
}

void AIMPManager::savePNGCoverToFile(TrackDescription track_desc, int cover_width, int cover_height, const std::wstring& filename) const
{
    try {
        using namespace ImageUtils;
        std::auto_ptr<AIMPCoverImage> cover( getCoverImage(track_desc, cover_width, cover_height) );
        cover->saveToFile(AIMPCoverImage::PNG_IMAGE, filename);
    } catch (std::exception& e) {
        const std::string& str = MakeString() << "Error occured while cover saving to file for " << track_desc << ". Reason: " << e.what();
        BOOST_LOG_SEV(logger(), error) << str;
        throw std::runtime_error(str);
    }
}

std::auto_ptr<ImageUtils::AIMPCoverImage> AIMPManager::getCoverImage(TrackDescription track_desc, int cover_width, int cover_height) const
{
    if (cover_width < 0 || cover_height < 0) {
        throw std::invalid_argument(MakeString() << "Error in "__FUNCTION__ << ". Negative cover size.");
    }

    const PlaylistEntry& entry = getEntry(track_desc);
    const SIZE request_full_size = { 0, 0 };
    HBITMAP cover_bitmap_handle = aimp_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry.getFilename().c_str() ), &request_full_size);

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

        cover_bitmap_handle = aimp_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry.getFilename().c_str() ), &cover_size);
    }

    using namespace ImageUtils;
    return std::auto_ptr<AIMPCoverImage>( new AIMPCoverImage(cover_bitmap_handle) ); // do not close handle of AIMP bitmap.
}

const Playlist& AIMPManager::getPlaylist(PlaylistID playlist_id) const
{
    if (playlist_id == -1) { // treat ID -1 as active playlist.
        playlist_id = getActivePlaylist();
    }

    auto playlist_iterator( playlists_.find(playlist_id) );
    if ( playlist_iterator == playlists_.end() ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__ << ": playlist with ID = " << playlist_id << " does not exist");
    }

    return playlist_iterator->second;
}

const PlaylistEntry& AIMPManager::getEntry(TrackDescription track_desc) const
{
    const Playlist& playlist = getPlaylist(track_desc.playlist_id);

    if (track_desc.track_id == -1) { // treat ID -1 as active track.
        if (   track_desc.playlist_id == -1
            || track_desc.playlist_id == getActivePlaylist()
            )
        {
            track_desc.track_id = getActiveEntry();
        }
    }
    const EntriesListType& entries = playlist.getEntries();
    if ( track_desc.track_id < 0 || static_cast<size_t>(track_desc.track_id) >= entries.size() ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__ << ". Entry " << track_desc << " does not exist");
    }

    return entries[track_desc.track_id]; // currently track ID is simple index in entries list.
}

void AIMPManager::notifyAboutInternalEvent(INTERNAL_EVENTS internal_event)
{
    switch (internal_event)
    {
    case VOLUME_EVENT:
        notifyAllExternalListeners(EVENT_VOLUME);
        break;
    case MUTE_EVENT:
        notifyAllExternalListeners(EVENT_MUTE);
        break;
    case SHUFFLE_EVENT:
        notifyAllExternalListeners(EVENT_SHUFFLE);
        break;
    case REPEAT_EVENT:
        notifyAllExternalListeners(EVENT_REPEAT);
        break;
    case PLAYLISTS_CONTENT_CHANGED_EVENT:
        notifyAllExternalListeners(EVENT_PLAYLISTS_CONTENT_CHANGE);
        break;
    case TRACK_PROGRESS_CHANGED_DIRECTLY_EVENT:
        notifyAllExternalListeners(EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY);
        break;
    default:
        assert(!"Unknown internal event in "__FUNCTION__);
        BOOST_LOG_SEV(logger(), info) << "Unknown internal event " << internal_event << " in "__FUNCTION__;
    }
}

void WINAPI AIMPManager::internalAIMPStateNotifier(DWORD User, DWORD dwCBType)
{
    AIMPManager* aimp_manager = reinterpret_cast<AIMPManager*>(User);
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
        aimp_manager->strand_.post( boost::bind(&AIMPManager::notifyAllExternalListeners,
                                                aimp_manager,
                                                event
                                                )
                                   );
    }
}

void AIMPManager::notifyAllExternalListeners(EVENTS event) const
{
    BOOST_FOREACH(const auto& listener_pair, external_listeners_) {
        const EventsListener& listener = listener_pair.second;
        listener(event);
    }
}

AIMPManager::EventsListenerID AIMPManager::registerListener(AIMPManager::EventsListener listener)
{
    external_listeners_[next_listener_id_] = listener;
    assert(next_listener_id_ != UINT32_MAX);
    return ++next_listener_id_; // get next unique listener ID using simple increment.
}

void AIMPManager::unRegisterListener(AIMPManager::EventsListenerID listener_id)
{
    external_listeners_.erase(listener_id);
}


namespace
{

struct BitrateFormatter {
    mutable std::wostringstream os;

    // need to define ctors since wostringstream has no copy ctor.
    BitrateFormatter() {}
    BitrateFormatter(const BitrateFormatter&) : os() {}

    std::wstring operator()(const PlaylistEntry& entry) const { 
        os.seekp(0, std::ios_base::beg);
        os << entry.getBitrate() << " kbps";
        return os.str();
    }
};

struct ChannelsCountFormatter {
    mutable std::wostringstream os;

    // need to define ctors since wostringstream has no copy ctor.
    ChannelsCountFormatter() {}
    ChannelsCountFormatter(const ChannelsCountFormatter&) : os() {}

    std::wstring operator()(const PlaylistEntry& entry) const { 
        os.seekp(0, std::ios_base::beg);
        switch ( entry.getChannelsCount() ) {
        case 0:
            break;
        case 1:
            os << L"Mono";
            break;
        case 2:
            os << L"Stereo";
            break;
        default:
            os << entry.getChannelsCount() << L" channels";
            break;
        }
        return os.str();
    }
};

struct DurationFormatter
{
    mutable std::wostringstream os;

    // need to define ctors since wostringstream has no copy ctor.
    DurationFormatter() {}
    DurationFormatter(const DurationFormatter&) : os() {}

    std::wstring operator()(const PlaylistEntry& entry) const {
        os.seekp(0, std::ios_base::beg);
        formatTime( os, entry.getDuration() );
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

struct FileNameExtentionFormatter {
    std::wstring operator()(const PlaylistEntry& entry) const { 
        std::wstring ext = boost::filesystem::extension( entry.getFilename() );
        if ( !ext.empty() ) {
            if (ext[0] == L'.') {
                ext.erase( ext.begin() );
            }
        }
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        return ext;
    }
};

struct SampleRateFormatter {
    mutable std::wostringstream os;

    // need to define ctors since wostringstream has no copy ctor.
    SampleRateFormatter() {}
    SampleRateFormatter(const SampleRateFormatter&) : os() {}

    std::wstring operator()(const PlaylistEntry& entry) const { 
        const DWORD rate_in_hertz = entry.getSampleRate();
        os.seekp(0, std::ios_base::beg);
        os << (rate_in_hertz / 1000)
           << " kHz";
        return os.str();
    }
};

struct FileSizeFormatter
{
    mutable std::wostringstream os;
    // need to define ctors since wostringstream has no copy ctor.
    FileSizeFormatter() {}
    FileSizeFormatter(const FileSizeFormatter&) : os() {}

    std::wstring operator()(const PlaylistEntry& entry) const {
        os.seekp(0, std::ios_base::beg);
        formatSize( os, entry.getFileSize() );
        return os.str();
    }

    static void formatSize(std::wostringstream& os, INT64 size_in_bytes) {
        const INT64 bytes_in_megabyte = 1024 * 1024;
        os.precision(3);
        if (size_in_bytes >= bytes_in_megabyte) {
            os << size_in_bytes / double(bytes_in_megabyte) << " Mb";
        } else {
            os << size_in_bytes / 1024.0 << " kb";
        }
    }
};

/*!
    \brief Helper class for AIMPManager::getFormattedEntryTitle() function.
           Implementation of AIMP title format analog.
*/
class PlaylistEntryTitleFormatter
{
public:
    PlaylistEntryTitleFormatter()
    {
#define _MAKE_FUNC_(function) boost::bind( createStringMakerFunctor(&function), boost::bind(&function,    _1) )
        using namespace boost::assign;
        insert(formatters_)
            ( 'A', _MAKE_FUNC_(PlaylistEntry::getAlbum) )
            ( 'a', _MAKE_FUNC_(PlaylistEntry::getArtist) )
            //( 'B', _MAKE_FUNC_(PlaylistEntry::getBitrate) ) // use BitrateFormatter which adds units(ex.: kbps)
            ( 'B', boost::bind<std::wstring>(BitrateFormatter(), _1) )
            //( 'C', _MAKE_FUNC_(PlaylistEntry::getChannelsCount) ) // use ChannelsCountFormatter which uses string representation (ex.: Mono/Stereo)
            ( 'C', boost::bind<std::wstring>(ChannelsCountFormatter(), _1) )
            ( 'E', boost::bind<std::wstring>(FileNameExtentionFormatter(), _1) )
            //( 'F', _MAKE_FUNC_(PlaylistEntry::getFilename) ) getting filename is disabled.
            ( 'G', _MAKE_FUNC_(PlaylistEntry::getGenre) )
            //( 'H', _MAKE_FUNC_(PlaylistEntry::getSampleRate) ) // this returns rate in Hertz, so use adequate SampleRateFormatter.
            ( 'H', boost::bind<std::wstring>(SampleRateFormatter(), _1) )
            //( 'L', _MAKE_FUNC_(PlaylistEntry::getDuration) ) // this returns milliseconds, so use adequate DurationFormatter.
            ( 'L', boost::bind<std::wstring>(DurationFormatter(), _1) )
            ( 'M', _MAKE_FUNC_(PlaylistEntry::getRating) )
            //( 'S', _MAKE_FUNC_(PlaylistEntry::getFileSize) ) // this returns size in bytes, so use adequate FileSizeFormatter.
            ( 'S', boost::bind<std::wstring>(FileSizeFormatter(), _1) )
            ( 'T', _MAKE_FUNC_(PlaylistEntry::getTitle) )
            ( 'Y', _MAKE_FUNC_(PlaylistEntry::getDate) )
        ;
        formatters_end_ = formatters_.end();
#undef _MAKE_FUNC_
    }

    static bool endOfFormatString(std::string::const_iterator curr_char,
                                  std::string::const_iterator end,
                                  char end_of_string
                                  )
    {
        return curr_char == end || *curr_char == end_of_string;
    }

    // returns count of characters read.
    size_t format(const PlaylistEntry& entry,
                  std::string::const_iterator begin,
                  std::string::const_iterator end,
                  char end_of_string,
                  std::wstring& formatted_string) const
    {
        auto curr_char = begin;
        while ( !endOfFormatString(curr_char, end, end_of_string) ) {
            if (*curr_char == format_argument_symbol) {
                if (curr_char + 1 != end) {
                    ++curr_char;
                    const auto formatter_it = formatters_.find(*curr_char);
                    if (formatter_it != formatters_end_) {
                        formatted_string += formatter_it->second(entry);
                        curr_char += 1; // go to char next to format argument.
                    } else {
                        switch(*curr_char) {
                        case '%':
                        case ',': // since ',' and ')' chars are used in expression "%IF(a, b, c)" we must escape them in usual string as '%,' '%)'.
                        case ')':
                            formatted_string.push_back(*curr_char++);
                            break;
                        default:
                            {
                            if (*curr_char == 'I')
                                if (curr_char != end && *++curr_char == 'F')
                                    if (curr_char != end && *++curr_char == '(') {
                                        // %IF(a, b, c): means a.empty() ? c : b;
                                        ++curr_char;
                                        std::wstring a;
                                        std::advance( curr_char, format(entry, curr_char, end, ',', a) ); // read a.
                                        ++curr_char;

                                        std::wstring b;
                                        std::advance( curr_char, format(entry, curr_char, end, ',', b) ); // read b.
                                        ++curr_char;

                                        std::wstring c;
                                        std::advance( curr_char, format(entry, curr_char, end, ')', c) ); // read c.
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

    std::wstring format(const PlaylistEntry& entry, const std::string& format_string) const // throw std::invalid_argument
    {
        const auto begin = format_string.begin(),
                   end   = format_string.end();
        std::wstring formatted_string;
        format(entry, begin, end, '\0', formatted_string);
        return formatted_string;
    }

private:

    template<class T>
    struct WStringMaker : std::unary_function<const T&, std::wstring> {
        std::wstring operator()(const T& arg) const
            { return boost::lexical_cast<std::wstring>(arg); }
    };

    template<class T, class R>
    WStringMaker<R> createStringMakerFunctor( R (T::*)() const )
        { return WStringMaker<R>(); }

    typedef boost::function<std::wstring(const PlaylistEntry&)> EntryFieldStringGetter;
    typedef std::map<char, EntryFieldStringGetter> Formatters;
    Formatters formatters_;
    Formatters::const_iterator formatters_end_;

    static const char format_argument_symbol = '%';

} playlistentry_title_formatter;

} // namespace anonymous

std::wstring AIMPManager::getFormattedEntryTitle(const PlaylistEntry& entry, const std::string& format_string) const // throw std::invalid_argument
{
    return playlistentry_title_formatter.format(entry, format_string);
}

std::ostream& operator<<(std::ostream& os, const TrackDescription& track_desc)
{
    os << "track: " << track_desc.track_id << ", playlist: " << track_desc.playlist_id;
    return os;
}

bool operator<(const TrackDescription& left, const TrackDescription& right)
{
    return (left.playlist_id < right.playlist_id) ? true
                                                  : (left.playlist_id == right.playlist_id) && (left.track_id < right.track_id) ? true
                                                                                                                                : false;
}

bool operator==(const TrackDescription& left, const TrackDescription& right)
{
    return (left.playlist_id == right.playlist_id) && (left.track_id == right.track_id);
}

} // namespace AIMPPlayer
