// Copyright (c) 2011, Alexey Ivanov

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
    */
    AIMP2Manager(boost::intrusive_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                 boost::asio::io_service& io_service_
                 ); // throws std::runtime_error

    ~AIMP2Manager();

    //! Starts playback.
    void startPlayback();

    /*!
        \brief Starts playback of specified track.
        \param track_desc track descriptor, see TrackDescription struct declaration for detailes.
        \throw std::runtime_error if track does not exist.
    */
    void startPlayback(TrackDescription track_desc); // throws std::runtime_error

    //! Stops playback.
    void stopPlayback();

    //! \return AIMP application version.
    std::string getAIMPVersion() const;

    //! Sets pause.
    void pausePlayback();

    //! Plays next track.
    void playNextTrack();

    //! Plays previous track.
    void playPreviousTrack();

    /*!
        \brief Sets AIMP status to specified value. See enum STATUS to know about available statuses and ranges of their values.
        \param status - status to set.
        \param value - depends on status.
        \throw std::runtime_error if status was not set successfully.
    */
    void setStatus(STATUS status, StatusValue value); // throws std::runtime_error

    //! Gets value for specified status.
    StatusValue getStatus(STATUS status) const;

    /*!
        \brief Enqueues specified track for playing.
        \param track_desc - track descriptor.
        \param insert_at_queue_beginning - flag, must be set to insert track at queue beginning, otherwise track will be set to end of queue.
        \throw std::runtime_error if track does not exist.
    */
    void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning); // throws std::runtime_error

    /*!
        \brief Removes track from AIMP play queue.
        \param track_desc - track descriptor.
        \throw std::runtime_error if track does not exist.
    */
    void removeEntryFromPlayQueue(TrackDescription track_desc); // throws std::runtime_error

    //! \return current playing playlist.
    PlaylistID getPlayingPlaylist() const;

    //! \return current playing track.
    PlaylistEntryID getPlayingEntry() const;

    //! \return descriptor of active track.
    TrackDescription getPlayingTrack() const;

    //! \return ID of playback state.
    PLAYBACK_STATE getPlaybackState() const;

    //! \return list of all playlists.
    const PlaylistsListType& getPlayLists() const;

    /*! \return reference to playlist with playlist_id ID. */
    const Playlist& getPlaylist(PlaylistID playlist_id) const; // throw std::runtime_error

    /*!
        \param track_desc - track descriptor.
        \return reference to entry.
        \throw std::runtime_error if track does not exist.
    */
    const PlaylistEntry& getEntry(TrackDescription track_desc) const; // throw std::runtime_error

    /*!
        \brief Returns formatted entry descrition string. Acts like printf() analog, see detailes below.
        \param entry - reference to entry.
        \param format_string - there are following format arguments:<BR>
        <PRE>
            %A - album
            %a - artist
            %B - bitrate
            %C - channels count
            //%F - path to file
            %G - genre
            %H - sample rate
            %L - duration
            %S - filesize
            %T - title
            %Y - date
            %M - rating
            %IF(A, B, C) - if A is empty use C else use B.
        Example: format_string = "%a - %T", result = "artist - title".
        </PRE>
        \return formatted string for entry.
    */
    std::wstring getFormattedEntryTitle(const PlaylistEntry& entry, const std::string& format_string_utf8) const;

    /*!
        \brief Saves album cover for track to std::vector in PNG format.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \param image_data - container for data store.
        \throw std::runtime_error in case of any error.
    */
    void savePNGCoverToVector(TrackDescription track_desc, int cover_width, int cover_height, std::vector<BYTE>& image_data) const; // throw std::runtime_error

    /*!
        \brief Saves album cover for track to file in PNG format.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \param filename - file name to store image.
        \throw std::runtime_error in case of any error.
    */
    void savePNGCoverToFile(TrackDescription track_desc, int cover_width, int cover_height, const std::wstring& filename) const; // throw std::runtime_error

    /*!
        \brief Registers notifier which will be called when specified event will occur.
               Client must call unRegisterListener() with gained ID to unsubscribe from notifications.
               It's important to call unRegisterListener() in listener dtor to avoid using of destroyed objects.
               Notification called from work thread of boost::asio::io_service object which is passed in AIMPManager ctor.
        \param event - type of event to notify.
        \param notifier - functor with signature 'void (EVENTS)'.
        \return listener ID that is used to unsubscribe from notifications.
    */
    EventsListenerID registerListener(EventsListener listener);

    /*! Removes listener with specified ID.
        \param notifier_id - listener ID that returned by registerListener() method call.
    */
    void unRegisterListener(EventsListenerID listener_id);

    void onTick();

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

} // namespace AIMPPlayer

#endif // #ifndef AIMP_MANAGER2_IMPL_H
