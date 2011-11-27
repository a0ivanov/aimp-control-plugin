// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "manager3_impl.h"
#include "manager_impl_common.h"
#include "playlist_entry.h"
#include "aimp3_sdk/aimp3_sdk.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/image.h"
#include "utils/util.h"
#include <boost/assign/std.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}


/* Specialization of Utilities::crc32(T) for AIMP3SDK::TAIMPFileInfo struct.
     Be sure that following assertion is ok:
        TAIMPFileInfo info;
        PlaylistEntry entry(info);
        assert( crc32(entry) == crc32(info) )
*/
template<>
crc32_t Utilities::crc32<AIMP3SDK::TAIMPFileInfo>(const AIMP3SDK::TAIMPFileInfo& info)
{
#define ENTRY_FIELD_CRC32(field) (info.field##Length == 0) ? 0 : Utilities::crc32( &info.field[0], info.field##Length * sizeof(info.field[0]) )
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
using namespace AIMP3SDK;

AIMP3Manager::AIMP3Manager(boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit)
    :
    aimp3_core_unit_(aimp3_core_unit),
    next_listener_id_(0)
{
    try {
        initializeAIMPObjects();
    } catch (std::runtime_error& e) {
        throw std::runtime_error( std::string("Error occured while AIMP3Manager initialization. Reason:") + e.what() );
    }
    ///!!!register listeners here
}

AIMP3Manager::~AIMP3Manager()
{
    ///!!!unregister listeners here
}

Playlist AIMP3Manager::loadPlaylist(int playlist_index)
{
    const char * const error_prefix = "Error occured while extracting playlist data: ";
    
    int id;
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_ID_By_Index(playlist_index, &id) ) {
        throw std::runtime_error(MakeString() << error_prefix << "AIMP_PLS_ID_By_Index failed");
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

void AIMP3Manager::loadEntries(Playlist& playlist) // throws std::runtime_error
{
    // PROFILE_EXECUTION_TIME(__FUNCTION__);
    const PlaylistID playlist_id = playlist.getID();
    const int entries_count = aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);

    AIMP2FileInfoHelper file_info_helper; // used for get entries from AIMP conviniently.

    // temp objects to prevent partial change state of passed objects when error occurs.
    EntriesListType entries;
    entries.reserve(entries_count);
    
    for (int entry_index = 0; entry_index < entries_count; ++entry_index) {
        // aimp2_playlist_manager_->AIMP_PLS_Entry_ReloadInfo(id_, entry_index); // try to make AIMP update track info: this takes significant time and some tracks are not updated anyway.
        if ( aimp2_playlist_manager_->AIMP_PLS_Entry_InfoGet(playlist_id,
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



void AIMP3Manager::startPlayback()
{
    // play current track.
    aimp2_player_->PlayOrResume();
}

void AIMP3Manager::startPlayback(TrackDescription track_desc) // throws std::runtime_error
{
    if ( FALSE == aimp2_player_->PlayTrack(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__" with " << track_desc );
    }
}

void AIMP3Manager::stopPlayback()
{
    aimp2_player_->Stop();
}

void AIMP3Manager::pausePlayback()
{
    aimp2_player_->Pause();
}

void AIMP3Manager::playNextTrack()
{
    aimp2_player_->NextTrack();
}

void AIMP3Manager::playPreviousTrack()
{
    aimp2_player_->PrevTrack();
}

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);
typedef int AIMP2SDK_STATUS;

void AIMP3Manager::notifyAboutInternalEventOnStatusChange(AIMP3Manager::STATUS status)
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

void AIMP3Manager::setStatus(AIMP3Manager::STATUS status, AIMP3Manager::StatusValue value)
{
    try {
        if ( FALSE == aimp2_controller_->AIMP_Status_Set(cast<AIMP2SDK_STATUS>(status), value) ) {
            throw std::runtime_error(MakeString() << "Error occured while setting status " << asString(status) << " to value " << value);
        }
    } catch (std::bad_cast& e) {
        throw std::runtime_error( e.what() );
    }

    notifyAboutInternalEventOnStatusChange(status);
}

AIMP3Manager::StatusValue AIMP3Manager::getStatus(AIMP3Manager::STATUS status) const
{
    return aimp2_controller_->AIMP_Status_Get(status);
}

std::string AIMP3Manager::getAIMPVersion() const
{
    const int version = aimp2_controller_->AIMP_GetSystemVersion(); // IAIMP2Player::Version() is not used since it is always returns 1. Maybe it is Aimp SDK version?
    using namespace std;
    ostringstream os;
    os << version / 1000 << '.' << setfill('0') << setw(2) << (version % 1000) / 10 << '.' << version % 10;
    return os.str();
}

PlaylistID AIMP3Manager::getActivePlaylist() const
{
    // return AIMP internal playlist ID here since AIMP3Manager uses the same IDs.
    return aimp2_playlist_manager_->AIMP_PLS_ID_PlayingGet();
}

PlaylistEntryID AIMP3Manager::getActiveEntry() const
{
    const PlaylistID active_playlist = getActivePlaylist();
    const int internal_active_entry_index = aimp2_playlist_manager_->AIMP_PLS_ID_PlayingGetTrackIndex(active_playlist);
    // internal index equals AIMP3Manager ID. In other case map index<->ID(use Playlist::entries_id_list_) here in all places where TrackDescription is used.
    const PlaylistEntryID entry_id = internal_active_entry_index;
    return entry_id;
}

TrackDescription AIMP3Manager::getActiveTrack() const
{
    return TrackDescription( getActivePlaylist(), getActiveEntry() );
}

AIMP3Manager::PLAYBACK_STATE AIMP3Manager::getPlaybackState() const
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

void AIMP3Manager::enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) // throws std::runtime_error
{
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_Entry_QueueSet(track_desc.playlist_id, track_desc.track_id, insert_at_queue_beginning) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

void AIMP3Manager::removeEntryFromPlayQueue(TrackDescription track_desc) // throws std::runtime_error
{
    if ( S_OK != aimp2_playlist_manager_->AIMP_PLS_Entry_QueueRemove(track_desc.playlist_id, track_desc.track_id) ) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__" with " << track_desc);
    }
}

const AIMP3Manager::PlaylistsListType& AIMP3Manager::getPlayLists() const
{
    return playlists_;
}

void AIMP3Manager::savePNGCoverToVector(TrackDescription track_desc, int cover_width, int cover_height, std::vector<BYTE>& image_data) const
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

void AIMP3Manager::savePNGCoverToFile(TrackDescription track_desc, int cover_width, int cover_height, const std::wstring& filename) const
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

std::auto_ptr<ImageUtils::AIMPCoverImage> AIMP3Manager::getCoverImage(TrackDescription track_desc, int cover_width, int cover_height) const
{
    if (cover_width < 0 || cover_height < 0) {
        throw std::invalid_argument(MakeString() << "Error in "__FUNCTION__ << ". Negative cover size.");
    }

    const PlaylistEntry& entry = getEntry(track_desc);
    const SIZE request_full_size = { 0, 0 };
    HBITMAP cover_bitmap_handle = aimp2_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry.getFilename().c_str() ), &request_full_size);

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

        cover_bitmap_handle = aimp2_cover_art_manager_->GetCoverArtForFile(const_cast<PWCHAR>( entry.getFilename().c_str() ), &cover_size);
    }

    using namespace ImageUtils;
    return std::auto_ptr<AIMPCoverImage>( new AIMPCoverImage(cover_bitmap_handle) ); // do not close handle of AIMP bitmap.
}

const Playlist& AIMP3Manager::getPlaylist(PlaylistID playlist_id) const
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

const PlaylistEntry& AIMP3Manager::getEntry(TrackDescription track_desc) const
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

void AIMP3Manager::notifyAllExternalListeners(EVENTS event) const
{
    BOOST_FOREACH(const auto& listener_pair, external_listeners_) {
        const EventsListener& listener = listener_pair.second;
        listener(event);
    }
}

AIMP3Manager::EventsListenerID AIMP3Manager::registerListener(AIMP3Manager::EventsListener listener)
{
    external_listeners_[next_listener_id_] = listener;
    assert(next_listener_id_ != UINT32_MAX);
    return ++next_listener_id_; // get next unique listener ID using simple increment.
}

void AIMP3Manager::unRegisterListener(AIMP3Manager::EventsListenerID listener_id)
{
    external_listeners_.erase(listener_id);
}

std::wstring AIMP3Manager::getFormattedEntryTitle(const PlaylistEntry& entry, const std::string& format_string) const // throw std::invalid_argument
{
    assert(!"to implement");
    return ""; ///!!!
}

} // namespace AIMPPlayer
