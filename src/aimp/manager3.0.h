// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include "manager.h"
#include "aimp3_sdk/aimp3_sdk.h"
#include "utils/util.h"

struct sqlite3;

namespace AIMP3SDK {
    class IAIMPCoreUnit;
    class IAIMPAddonsPlayerManager;
    class IAIMPAddonsPlaylistManager;
    class IAIMPAddonsCoverArtManager;
}

namespace AimpRpcMethods {
    class EmulationOfWebCtlPlugin;
}

namespace AIMPPlayer
{

/*!
    \brief Provides interaction with AIMP3 player.
*/
class AIMPManager30 : public AIMPManager
{
public:

    /*!
        \param aimp3_core_unit - pointer to IAIMPCoreUnit object.
    */
    AIMPManager30(boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit); // throws std::runtime_error

    virtual ~AIMPManager30();

    virtual void startPlayback();

    virtual void startPlayback(TrackDescription track_desc); // throws std::runtime_error

    virtual void stopPlayback();

    virtual std::string getAIMPVersion() const;

    virtual void pausePlayback();

    virtual void playNextTrack();

    virtual void playPreviousTrack();

    virtual void setStatus(STATUS status, StatusValue value); // throws std::runtime_error

    virtual StatusValue getStatus(STATUS status) const;

    virtual void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning); // throws std::runtime_error

    virtual void removeEntryFromPlayQueue(TrackDescription track_desc); // throws std::runtime_error

    virtual PlaylistID getPlayingPlaylist() const;

    virtual PlaylistEntryID getPlayingEntry() const;

    virtual TrackDescription getPlayingTrack() const;

    virtual PlaylistID getAbsolutePlaylistID(PlaylistID id) const;

    virtual PlaylistEntryID getAbsoluteEntryID(PlaylistEntryID id) const; // throws std::runtime_error

    virtual TrackDescription getAbsoluteTrackDesc(TrackDescription track_desc) const; // throws std::runtime_error

    virtual crc32_t getPlaylistCRC32(PlaylistID playlist_id) const; // throws std::runtime_error

    virtual PLAYLIST_ENTRY_SOURCE_TYPE getTrackSourceType(TrackDescription track_desc) const; // throws std::runtime_error

    virtual PLAYBACK_STATE getPlaybackState() const;

    virtual std::wstring getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const;

    virtual bool isCoverImageFileExist(TrackDescription track_desc, boost::filesystem::wpath* path = nullptr) const;

    virtual void saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width = 0, int cover_height = 0) const; // throw std::runtime_error
    
    virtual EventsListenerID registerListener(EventsListener listener);

    virtual void unRegisterListener(EventsListenerID listener_id);

    virtual void onTick();

    virtual int trackRating(TrackDescription track_desc) const; // throws std::runtime_error

    virtual void addFileToPlaylist(const boost::filesystem::wpath& path, PlaylistID playlist_id); // throws std::runtime_error
    
    virtual void addURLToPlaylist(const std::string& url, PlaylistID playlist_id); // throws std::runtime_error

    virtual PlaylistID createPlaylist(const std::wstring& title);

    // AIMP3 specific functionality, not supported by AIMP2.

    /*!
        Sets rating of specified track.

        \param track_desc - track descriptor.
        \param rating - rating value is in range [0-5]. Zero value means rating is not set.
    */
    virtual void trackRating(TrackDescription track_desc, int rating); // throw std::runtime_error

    sqlite3* playlists_db()
        { return playlists_db_; }
    sqlite3* playlists_db() const
        { return playlists_db_; }

private:

    void onAimpCoreMessage(DWORD AMessage, int AParam1, void *AParam2, HRESULT *AResult);
    void onStorageActivated(AIMP3SDK::HPLS handle);
    void onStorageAdded(AIMP3SDK::HPLS handle);
    void onStorageChanged(AIMP3SDK::HPLS handle, DWORD flags);
    void onStorageRemoved(AIMP3SDK::HPLS handle);

    /*!
        \brief Return album cover for track_id in playlist_id.
        -Size is determined by cover_width and cover_height arguments:
            -# Pass zeros to get full size cover.
            -# Pass zero height and non zero width to calc proportional height.
            -# Pass zero width and non zero height to calc proportional width.
            -# Pass non zero width and height to get any image size(may be stretched).
        \throw std::runtime_error - if image can not be created.
        \throw std::invalid_argument - if parameters cover_width or/and cover_height are invalid.
    */
    std::auto_ptr<ImageUtils::AIMPCoverImage> getCoverImage(TrackDescription track_desc, int cover_width, int cover_height) const; // throw std::runtime_error, throw std::invalid_argument

    //! Called from setStatus() and invokes notifyAboutInternalEvent() to notify about status changes which AIMP does not notify us about.
    void notifyAboutInternalEventOnStatusChange(STATUS status);

    /*!
        Notifies all registered listeners.
        Note: function is invoked from thread linked with strand_ member.
    */
    void notifyAllExternalListeners(EVENTS event) const;

    /*!
        \brief Loads playlist entries from AIMP.
        \throw std::invalid_argument if playlist with specified ID does not exist.
        \throw std::runtime_error if error occured while loading entries data.
    */
    void loadEntries(PlaylistID playlist_id); // throws std::runtime_error

    //! Loads playlist by AIMP internal index.
    void loadPlaylist(int playlist_index); // throws std::runtime_error
    void loadPlaylist(AIMP3SDK::HPLS handle); // throws std::runtime_error

    void initPlaylistDB(); // throws std::runtime_error
    void shutdownPlaylistDB();
    void deletePlaylistEntriesFromPlaylistDB(PlaylistID playlist_id);
    void deletePlaylistFromPlaylistDB(PlaylistID playlist_id);
    void updatePlaylistCrcInDB(PlaylistID playlist_id, crc32_t crc32); // throws std::runtime_error
    
    //! initializes all requiered for work AIMP SDK interfaces.
    void initializeAIMPObjects(); // throws std::runtime_error

    // pointers to internal AIMP3 objects.

protected:
    boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit>              aimp3_core_unit_;
    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlayerManager>   aimp3_player_manager_;
    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistManager> aimp3_playlist_manager_;
private:
    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsCoverArtManager> aimp3_coverart_manager_;

    class AIMPCoreUnitMessageHook;
    boost::intrusive_ptr<AIMPCoreUnitMessageHook> aimp3_core_message_hook_;
    class AIMPAddonsPlaylistManagerListener;
    boost::intrusive_ptr<AIMPAddonsPlaylistManagerListener> aimp3_playlist_manager_listener_;

    // types for notifications of external event listeners.
    typedef std::map<EventsListenerID, EventsListener> EventListeners;

    EventListeners external_listeners_; //!< map of all subscribed listeners.
    EventsListenerID next_listener_id_; //!< unique ID describes external listener.

protected:
    sqlite3* playlists_db_;

private:
    
    PlaylistCRC32& getPlaylistCRC32Object(PlaylistID playlist_id) const; // throws std::runtime_error
    typedef std::map<PlaylistID, PlaylistCRC32> PlaylistCRC32List;
    mutable PlaylistCRC32List playlist_crc32_list_;


    // These class were made friend only for easy emulate web ctl plugin behavior. Remove when possible.
    friend class AimpRpcMethods::EmulationOfWebCtlPlugin;
};

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);

template<>
PlaylistID cast(AIMP3SDK::HPLS handle);

template<>
AIMP3SDK::HPLS cast(PlaylistID id);

// use usual functions instead template specialization since in fact PlaylistEntryID == PlaylistID == int.
PlaylistEntryID castToPlaylistEntryID (AIMP3SDK::HPLSENTRY handle);

AIMP3SDK::HPLSENTRY castToHPLSENTRY(PlaylistEntryID id);

template<typename T>
T getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id);

template<>
std::wstring getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id);

template<>
DWORD getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id);

template<>
INT64 getEntryField(sqlite3* db, const char* field, PlaylistEntryID entry_id);

} // namespace AIMPPlayer
