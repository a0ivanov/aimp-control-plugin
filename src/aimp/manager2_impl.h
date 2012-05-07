// Copyright (c) 2012, Alexey Ivanov

#ifndef AIMP_MANAGER2_IMPL_H
#define AIMP_MANAGER2_IMPL_H

#include "manager.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>

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
class AIMP2Manager : public AIMPManager
{
public:

    /*!
        \param aimp_controller - pointer to IAIMP2Controller object.
        \param io_service_ - boost::asio::io_service, used to access to timer.
    */
    AIMP2Manager(boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                 boost::asio::io_service& io_service_
                 ); // throws std::runtime_error

    virtual ~AIMP2Manager();

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

    virtual PLAYBACK_STATE getPlaybackState() const;

    virtual const PlaylistsListType& getPlayLists() const;

    virtual const Playlist& getPlaylist(PlaylistID playlist_id) const; // throw std::runtime_error

    virtual const PlaylistEntry& getEntry(TrackDescription track_desc) const; // throw std::runtime_error

    virtual std::wstring getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const; // throw std::invalid_argument
    
    virtual void savePNGCoverToVector(TrackDescription track_desc, int cover_width, int cover_height, std::vector<BYTE>& image_data) const; // throw std::runtime_error

    virtual void savePNGCoverToFile(TrackDescription track_desc, int cover_width, int cover_height, const std::wstring& filename) const; // throw std::runtime_error

    virtual EventsListenerID registerListener(EventsListener listener);

    virtual void unRegisterListener(EventsListenerID listener_id);

    virtual void onTick();

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
        Internal AIMPManager events that occured inside member functions call.
        These are the events that AIMP does not notify about.
    */
    enum INTERNAL_EVENTS { VOLUME_EVENT,
                           MUTE_EVENT,
                           SHUFFLE_EVENT,
                           REPEAT_EVENT,
                           PLAYLISTS_CONTENT_CHANGED_EVENT,
                           TRACK_PROGRESS_CHANGED_DIRECTLY_EVENT // This event occurs when user himself change track progress bar.
                                                                 // Normal progress bar change(when track playing) does not generate this event.
    };

    //! Notify external subscribers about internal events.
    void notifyAboutInternalEvent(INTERNAL_EVENTS internal_event);

    //! Called from setStatus() and invokes notifyAboutInternalEvent() to notify about status changes which AIMP does not notify us about.
    void notifyAboutInternalEventOnStatusChange(STATUS status);

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

    /*!
        \brief Loads playlist entries from AIMP.
        \throw std::invalid_argument if playlist with specified ID does not exist.
        \throw std::runtime_error if error occured while loading entries data.
    */
    void loadEntries(Playlist& playlist); // throws std::runtime_error

    //! Loads playlist by AIMP internal index.
    Playlist loadPlaylist(int playlist_index); // throws std::runtime_error

    //! initializes all requiered for work AIMP SDK interfaces.
    void initializeAIMPObjects(); // throws std::runtime_error

    // pointers to internal AIMP2 objects.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller>       aimp2_controller_;        //!< to work with AIMP application.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Player>           aimp2_player_;            //!< to work with AIMP control panel.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp2_playlist_manager_;  //!< to work with playlists and tracks.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2Extended>         aimp2_extended_;          //!< to work aimp miscellaneous aspects.
    boost::intrusive_ptr<AIMP2SDK::IAIMP2CoverArtManager>  aimp2_cover_art_manager_; //!< to work with track's album covers.

    PlaylistsListType playlists_; //!< playlists list. Currently it is map of playlist ID to Playlist object.

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

    // types for notifications of external event listeners.
    typedef std::map<EventsListenerID, EventsListener> EventListeners;

    EventListeners external_listeners_; //!< map of all subscribed listeners.
    EventsListenerID next_listener_id_; //!< unique ID describes external listener.


    // These class were made friend only for easy emulate web ctl plugin behavior. Remove when possible.
    friend class AimpRpcMethods::EmulationOfWebCtlPlugin;
};

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);
typedef int AIMP2SDK_STATUS;

template<>
AIMP2SDK_STATUS cast(AIMPManager::STATUS status); // throws std::bad_cast

template<>
AIMP2Manager::STATUS cast(AIMP2SDK_STATUS status); // throws std::bad_cast

} // namespace AIMPPlayer

#endif // #ifndef AIMP_MANAGER2_IMPL_H
