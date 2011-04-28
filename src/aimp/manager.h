// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_MANAGER_H
#define AIMP_MANAGER_H

#include "config.h"
#include <unknwn.h>
#include "playlist.h"
#include <boost/function.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/asio.hpp>

namespace ImageUtils { class AIMPCoverImage; }

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

//! provide interface to interact with AIMP player.
namespace AIMPPlayer
{

struct TrackDescription;

/*! \brief Provides interaction with AIMP player.
           Hides low level AIMP SDK from application.
*/
class AIMPManager : boost::noncopyable
{
public:

    enum STATUS {
        STATUS_FIRST, // not a status.
        STATUS_VOLUME = 1, // volume level range is [0, 100].
        STATUS_BALANCE,
        STATUS_SPEED,
        STATUS_Player,
        STATUS_MUTE,       // true/false
        STATUS_REVERB,
        STATUS_ECHO,
        STATUS_CHORUS,
        STATUS_Flanger,

        STATUS_EQ_STS,
        STATUS_EQ_SLDR01,
        STATUS_EQ_SLDR02,
        STATUS_EQ_SLDR03,
        STATUS_EQ_SLDR04,
        STATUS_EQ_SLDR05,
        STATUS_EQ_SLDR06,
        STATUS_EQ_SLDR07,
        STATUS_EQ_SLDR08,
        STATUS_EQ_SLDR09,
        STATUS_EQ_SLDR10,
        STATUS_EQ_SLDR11,
        STATUS_EQ_SLDR12,
        STATUS_EQ_SLDR13,
        STATUS_EQ_SLDR14,
        STATUS_EQ_SLDR15,
        STATUS_EQ_SLDR16,
        STATUS_EQ_SLDR17,
        STATUS_EQ_SLDR18,

        STATUS_REPEAT,     // true/false
        STATUS_ON_STOP,
        STATUS_POS,
        STATUS_LENGTH,
        STATUS_REPEATPLS,
        STATUS_REP_PLS_1,
        STATUS_KBPS,
        STATUS_KHZ,
        STATUS_MODE,
        STATUS_RADIO,
        STATUS_STREAM_TYPE,
        STATUS_TIMER,
        STATUS_SHUFFLE,    // true/false
        STATUS_MAIN_HWND,
        STATUS_TC_HWND,
        STATUS_APP_HWND,
        STATUS_PL_HWND,
        STATUS_EQ_HWND,
        STATUS_TRAY,
        STATUS_LAST // not a status.
    };

    typedef int StatusValue;

    //! playback state identificators.
    enum PLAYBACK_STATE { STOPPED = 0, PLAYING, PAUSED, PLAYBACK_STATES_COUNT };

    enum EVENTS {
        EVENT_STATUS_CHANGE = 0,
        EVENT_PLAY_FILE,
        EVENT_INFO_UPDATE,
        EVENT_PLAYER_STATE,
        EVENT_EFFECT_CHANGED,
        EVENT_EQ_CHANGED,
        EVENT_TRACK_POS_CHANGED,

        EVENT_PLAYLISTS_CONTENT_CHANGE,
        EVENT_VOLUME,
        EVENT_MUTE,
        EVENT_SHUFFLE,
        EVENT_REPEAT,
        EVENTS_COUNT // not an event
    };
    typedef boost::function<void (EVENTS)> EventsListener;
    typedef std::size_t EventsListenerID;

    typedef std::map<PlaylistID, boost::shared_ptr<Playlist> > PlaylistsListType;

    /*!
        \param aimp_controller - pointer to IAIMP2Controller object.
    */
    AIMPManager(boost::shared_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller,
                boost::asio::io_service& io_service_
                ); // throws std::runtime_error

    ~AIMPManager();

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
        \brief Sets AIMP status to specified value. See enum STATUS to know about avaiable statuses and ranges of their values.
        \param status - status to set.
        \param value - depends on status.
        \throw std::runtime_error if status was not set successfully.
    */
    void setStatus(STATUS status, StatusValue value); // throws std::runtime_error

    //! Gets value for specified status.
    StatusValue getStatus(STATUS status) const;

    //! \return current active playlist.
    PlaylistID getActivePlaylist() const;

    //! \return current active track.
    PlaylistEntryID getActiveEntry() const;

    //! \return descriptor of active track.
    TrackDescription getActiveTrack() const;

    //! \return ID of playback state.
    PLAYBACK_STATE getPlaybackState() const;

    //! \return list of all playlists.
    const PlaylistsListType& getPlayLists() const;

    /*!
        \brief Enqueues specified track for playing.
        \param track_desc - track descriptor.
        \param insert_at_queue_beginning - flag, must be set to insert track at queue beggining, otherwise track will be set to end of queue.
        \throw std::runtime_error if track does not exist.
    */
    void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning); // throws std::runtime_error

    /*!
        \brief Removes track from AIMP play queue.
        \param track_desc - track descriptor.
        \throw std::runtime_error if track does not exist.
    */
    void removeEntryFromPlayQueue(TrackDescription track_desc); // throws std::runtime_error

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

    /*! \return reference to playlist with playlist_id ID. */
    const Playlist& getPlaylist(PlaylistID playlist_id) const; // throw std::runtime_error

    /*!
        \param track_desc - track descriptor.
        \return reference to entry.
        \throw std::runtime_error if track does not exist.
    */
    const PlaylistEntry& getEntry(TrackDescription track_desc) const; // throw std::runtime_error

    /*!
        \brief Returns formatted entrie's descrition string. Acts like printf() analog, see detailes below.
        \param entry - reference to entry.
        \param format_string - there are following format arguments:<BR>
        <PRE>
            %A - album
            %a - artist
            %B - bitrate
            %C - channels count
            //( 'F', _MAKE_FUNC_(std::wstring, PlaylistEntry::getFilename) )
            %G - genre
            %H - sample rate
            %L - duration
            %S - filesize
            %T - title
            %Y - date
            %M - rating
        Example: format_string = "%a - %T", result = "artist - title".
        </PRE>
        \return formatted string for entry.
    */
    std::wstring getFormattedEntryTitle(const PlaylistEntry& entry, const std::string& format_string) const;

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
                           PLAYLISTS_CONTENT_CHANGED_EVENT
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
    boost::shared_ptr<Playlist> loadPlaylist(int playlist_index); // throws std::runtime_error

    //! initializes all requiered for work AIMP SDK interfaces.
    void initializeAIMPObjects(); // throws std::runtime_error

    // pointers to internal AIMP objects.
    boost::shared_ptr<AIMP2SDK::IAIMP2Controller> aimp_controller_; //!< interface to work with AIMP application.
    boost::shared_ptr<AIMP2SDK::IAIMP2Player> aimp_player_; //!< interface to work with AIMP control panel.
    boost::shared_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager_; //!< interface for work with playlists.
    boost::shared_ptr<AIMP2SDK::IAIMP2Extended> aimp_extended_; //!< interface for work aimp miscellaneous aspects.
    boost::shared_ptr<AIMP2SDK::IAIMP2CoverArtManager> aimp_cover_art_manager_; //!< interface for work with track's album covers.

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


/*!
    \brief Functor to be used with boost::sharedptr<IAIMP2XXXX> objects;
           Calls IUnknown::Release() for passed object.
*/
struct IReleaseFunctor {
    void operator()(IUnknown* object) const;
};


/*!
    Track description.
    Track's absolute coordinates are { playlist ID, track ID in playlist }.
*/
struct TrackDescription
{
    TrackDescription(PlaylistEntryID track_id, PlaylistID playlist_id)
        :
        track_id(track_id),
        playlist_id(playlist_id)
    {}

    PlaylistEntryID track_id;
    PlaylistID playlist_id;
};

std::ostream& operator<<(std::ostream& os, const TrackDescription& track_desc);
bool operator<(const TrackDescription& left, const TrackDescription& right);
bool operator==(const TrackDescription& left, const TrackDescription& right);

inline bool aimpStatusValid(int status)
    { return (AIMPManager::STATUS_FIRST < status && status < AIMPManager::STATUS_LAST); }

} // namespace AIMPPlayer

#endif // #ifndef AIMP_MANAGER_H
