// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include "manager.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include "utils/util.h"

struct sqlite3;

namespace AIMP2SDK {
    class IAIMP2Controller;
    class IAIMP2Player;
    class IAIMP2PlaylistManager2;
    class IAIMP2Extended;
    class IAIMP2CoverArtManager;
}

namespace AimpRpcMethods {
    class EmulationOfWebCtlPlugin;
}

namespace AIMPPlayer
{

/*!
    \brief Provides interaction with AIMP2 player.
*/
class AIMPManager26 : public AIMPManager
{
public:

    /*!
        \param aimp_controller - pointer to IAIMP2Controller object.
        \param io_service_ - boost::asio::io_service, used to access to timer.
    */
    AIMPManager26(boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                  boost::asio::io_service& io_service_
                  ); // throws std::runtime_error

    virtual ~AIMPManager26();

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

    virtual TrackDescription getAbsoluteTrackDesc(TrackDescription track_desc) const;  // throws std::runtime_error

    virtual crc32_t getPlaylistCRC32(PlaylistID playlist_id) const; // throws std::runtime_error

    virtual PLAYLIST_ENTRY_SOURCE_TYPE getTrackSourceType(TrackDescription track_desc) const; // throws std::runtime_error

    virtual PLAYBACK_STATE getPlaybackState() const;

    //virtual PlaylistEntry getEntry(TrackDescription track_desc) const; // throw std::runtime_error

    virtual std::wstring getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const; // throw std::invalid_argument
    
    virtual std::wstring getEntryFilename(TrackDescription track_desc) const; // throw std::invalid_argument

    virtual bool isCoverImageFileExist(TrackDescription /*track_desc*/, boost::filesystem::wpath* path = nullptr) const
        { (void)path; return false; } // unsupported by AIMP 2 SDK.

    virtual int trackRating(TrackDescription track_desc) const; // throws std::runtime_error

    virtual void addFileToPlaylist(const boost::filesystem::wpath& path, PlaylistID playlist_id); // throws std::runtime_error
    
    virtual void addURLToPlaylist(const std::string& url, PlaylistID playlist_id); // throws std::runtime_error

    virtual PlaylistID createPlaylist(const std::wstring& title);

    virtual void saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width = 0, int cover_height = 0) const; // throw std::runtime_error

    virtual void removeTrack(TrackDescription track_desc, bool physically = false); // throws std::runtime_error

    virtual EventsListenerID registerListener(EventsListener listener);

    virtual void unRegisterListener(EventsListenerID listener_id);

    virtual void onTick();

    sqlite3* playlists_db()
        { return playlists_db_; }

    sqlite3* playlists_db() const
        { return playlists_db_; }

private:

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

    /*!
        Notifies all registered listeners.
        Note: function is invoked from thread linked with strand_ member.
    */
    void notifyAllExternalListeners(EVENTS event) const;

    //! Subscribes for all available AIMP callbacks.
    void registerNotifiers();

    //! Unsubscribes from all available AIMP callbacks.
    void unregisterNotifiers();

    //! Callback function to notify manager about AIMP player state changes. Called by AIMP from it's thread.
    static void WINAPI internalAIMPStateNotifier(DWORD User, DWORD dwCBType);

    static bool getEventRelatedTo(AIMPManager26::STATUS status, AIMPManager26::EVENTS* event);

    /*!
        \brief Loads playlist entries from AIMP.
        \throw std::invalid_argument if playlist with specified ID does not exist.
        \throw std::runtime_error if error occured while loading entries data.
    */
    void loadEntries(PlaylistID playlist_id); // throws std::runtime_error

    //! Loads playlist by AIMP internal index.
    void loadPlaylist(int playlist_index); // throws std::runtime_error

    //! initializes all requiered for work AIMP SDK interfaces.
    void initializeAIMPObjects(); // throws std::runtime_error

    // pointers to internal AIMP2 objects.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller>       aimp2_controller_;        //!< to work with AIMP application.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Player>           aimp2_player_;            //!< to work with AIMP control panel.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp2_playlist_manager_;  //!< to work with playlists and tracks.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Extended>         aimp2_extended_;          //!< to work aimp miscellaneous aspects.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2CoverArtManager>  aimp2_cover_art_manager_; //!< to work with track's album covers.

    //! type for internal AIMP events notifiers. Maps internal AIMP event ID to string description of this ID.
    typedef std::map<int, std::string> CallbackIdNameMap;

    CallbackIdNameMap aimp_callback_names_; //! Map of internal AIMP event ID to string description of this ID.

    boost::asio::io_service::strand strand_; // allows handle events of internalAIMPStateNotifier() function, which called from Aimp thread, in thread of using AIMPManager(those thread where boost::asio::io_service object passed AIMPManager's ctor is executed).

#ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION // code responsible for determination of playlists content changes.
    void onTimerPlaylistsChangeCheck(const boost::system::error_code& error);

    void checkIfPlaylistsChanged();

    //! Returns true if loaded playlist and playlist in AIMP are equal.
    bool isLoadedPlaylistEqualsAimpPlaylist(PlaylistID playlist_id) const;

    static const int kPLAYLISTS_CHECK_PERIOD_SEC = 5; // we check playlist changes each 5 seconds.
    boost::asio::deadline_timer playlists_check_timer_;

#endif // #ifdef MANUAL_PLAYLISTS_CONTENT_CHANGES_DETERMINATION

    void initPlaylistDB(); // throws std::runtime_error
    void shutdownPlaylistDB();
    void deletePlaylistEntriesFromPlaylistDB(PlaylistID playlist_id);
    void deletePlaylistFromPlaylistDB(PlaylistID playlist_id);
    void updatePlaylistCrcInDB(PlaylistID playlist_id, crc32_t crc32); // throws std::runtime_error

    // types for notifications of external event listeners.
    typedef std::map<EventsListenerID, EventsListener> EventListeners;

    EventListeners external_listeners_; //!< map of all subscribed listeners.
    EventsListenerID next_listener_id_; //!< unique ID describes external listener.

    sqlite3* playlists_db_;

    PlaylistCRC32& getPlaylistCRC32Object(PlaylistID playlist_id) const; // throws std::runtime_error
    typedef std::map<PlaylistID, PlaylistCRC32> PlaylistCRC32List;
    mutable PlaylistCRC32List playlist_crc32_list_;

    // These class were made friend only for easy emulate web ctl plugin behavior. Remove when possible.
    friend class AimpRpcMethods::EmulationOfWebCtlPlugin;
};

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);
typedef int AIMP2SDK_STATUS;

template<>
AIMP2SDK_STATUS cast(AIMPManager::STATUS status); // throws std::bad_cast

template<>
AIMPManager26::STATUS cast(AIMP2SDK_STATUS status); // throws std::bad_cast

template<typename T>
T getEntryField(sqlite3* db, const char* field, TrackDescription track_desc);

template<>
std::wstring getEntryField(sqlite3* db, const char* field, TrackDescription track_desc);

template<>
DWORD getEntryField(sqlite3* db, const char* field, TrackDescription track_desc);

template<>
INT64 getEntryField(sqlite3* db, const char* field, TrackDescription track_desc);

} // namespace AIMPPlayer
