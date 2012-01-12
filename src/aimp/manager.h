// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_MANAGER_H
#define AIMP_MANAGER_H

#include "config.h"
#include "playlist.h"
#include "track_description.h"
#include <boost/function.hpp>

namespace ImageUtils { class AIMPCoverImage; }

//! provide interface to interact with AIMP player.
namespace AIMPPlayer
{

/*! \brief Interface to interact with AIMP player.
           Hides low level AIMP SDK from application.
*/
class AIMPManager : boost::noncopyable
{
public:

    enum STATUS {
        STATUS_FIRST, // not a status.
        STATUS_VOLUME = 1, // volume level range is [0,100] in percents: 0 = 0%, 100 = 100%.
        STATUS_BALANCE, // [0,100]: 0 left only, 50 left=right, 100 right.
        STATUS_SPEED, // [0,100] in percents: 0 = 50%, 50 = 100%, 100 = 150%.
        STATUS_Player, // [0,1,2] stop, play, pause.
        STATUS_MUTE, // [0,1] off/on.
        STATUS_REVERB, // [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%.
        STATUS_ECHO, // [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%.
        STATUS_CHORUS, // [0,100]: 0 disabled, 10=1%, 49=54%, 100=99%.
        STATUS_Flanger, // [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%.

        STATUS_EQ_STS, // [0,1] off/on.
        STATUS_EQ_SLDR01, // EQ_SLDRXX [0,100]: 0 = -15db, 50 = 0db, 100 = 15db.
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

        STATUS_REPEAT, // [0,1] off/on.
        STATUS_STAY_ON_TOP, // [0,1] off/on.
        STATUS_POS, // POS [0,STATUS_LENGTH's value] in seconds.
        STATUS_LENGTH, // in seconds.
        STATUS_REPEATPLS, // [0,1,2] On the end of playlist "Jump to the next playlist"/"Repeat playlist"/"Do nothing
        STATUS_REP_PLS_1, // [0,1] Disable repeat when only one file in playlist Enable/Disable.
        STATUS_KBPS, // bitrate in kilobits/seconds.
        STATUS_KHZ, // sampling in Hertz, ex.: 44100.
        STATUS_MODE,
        STATUS_RADIO,
        STATUS_STREAM_TYPE,
        STATUS_REVERSETIME, // [0,1] off/on.
        STATUS_SHUFFLE, // [0,1] off/on.
        STATUS_MAIN_HWND,
        STATUS_TC_HWND,
        STATUS_APP_HWND,
        STATUS_PL_HWND,
        STATUS_EQ_HWND,
        STATUS_TRAY, // [0,1] false/true.
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
        EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY,
        EVENT_VOLUME,
        EVENT_MUTE,
        EVENT_SHUFFLE,
        EVENT_REPEAT,
        EVENTS_COUNT // not an event
    };

    typedef boost::function<void (EVENTS)> EventsListener;
    typedef std::size_t EventsListenerID;

    typedef std::map<PlaylistID, Playlist> PlaylistsListType;

    virtual ~AIMPManager() {}

    //! Starts playback.
    virtual void startPlayback() = 0;

    /*!
        \brief Starts playback of specified track.
        \param track_desc track descriptor, see TrackDescription struct declaration for detailes.
        \throw std::runtime_error if track does not exist.
    */
    virtual void startPlayback(TrackDescription track_desc) = 0; // throws std::runtime_error

    //! Stops playback.
    virtual void stopPlayback() = 0;

    //! \return AIMP application version.
    virtual std::string getAIMPVersion() const = 0;

    //! Sets pause.
    virtual void pausePlayback() = 0;

    //! Plays next track.
    virtual void playNextTrack() = 0;

    //! Plays previous track.
    virtual void playPreviousTrack() = 0;

    //! Gets value for specified status.
    virtual StatusValue getStatus(STATUS status) const = 0;

    /*!
        \brief Sets AIMP status to specified value. See enum STATUS to know about available statuses and ranges of their values.
        \param status - status to set.
        \param value - depends on status.
        \throw std::runtime_error if status was not set successfully.
    */
    virtual void setStatus(STATUS status, StatusValue value) = 0; // throws std::runtime_error

    /*!
        \brief Enqueues specified track for playing.
        \param track_desc - track descriptor.
        \param insert_at_queue_beginning - flag, must be set to insert track at queue beginning, otherwise track will be set to end of queue.
        \throw std::runtime_error if track does not exist.
    */
    virtual void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning) = 0; // throws std::runtime_error

    /*!
        \brief Removes track from AIMP play queue.
        \param track_desc - track descriptor.
        \throw std::runtime_error if track does not exist.
    */
    virtual void removeEntryFromPlayQueue(TrackDescription track_desc) = 0; // throws std::runtime_error

    //! \return current playing playlist.
    virtual PlaylistID getPlayingPlaylist() const = 0;

    //! \return current playing track.
    virtual PlaylistEntryID getPlayingEntry() const = 0; // throws std::runtime_error

    //! \return descriptor of playing track.
    virtual TrackDescription getPlayingTrack() const = 0;

    //! \return ID of playback state.
    virtual PLAYBACK_STATE getPlaybackState() const = 0;

    //! \return list of all playlists.
    virtual const PlaylistsListType& getPlayLists() const = 0;

    /*! \return reference to playlist with playlist_id ID. */
    virtual const Playlist& getPlaylist(PlaylistID playlist_id) const = 0; // throw std::runtime_error

    /*!
        \param track_desc - track descriptor.
        \return reference to entry.
        \throw std::runtime_error if track does not exist.
    */
    virtual const PlaylistEntry& getEntry(TrackDescription track_desc) const = 0; // throw std::runtime_error

    /*!
        \brief Returns formatted entry descrition string. Acts like printf() analog, see detailes below.
        \param entry - reference to entry.
        \param format_string - utf8 encoded string.
                               There are following format arguments:<BR>
        <PRE>
            %A - album
            %a - artist
            %B - bitrate
            %C - channels count
            %F - path to file
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
    virtual std::wstring getFormattedEntryTitle(const PlaylistEntry& entry, const std::string& format_string_utf8) const = 0;

    /*!
        \brief Saves album cover for track to std::vector in PNG format.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \param image_data - container for data store.
        \throw std::runtime_error in case of any error.
    */
    virtual void savePNGCoverToVector(TrackDescription track_desc, int cover_width, int cover_height, std::vector<BYTE>& image_data) const = 0; // throw std::runtime_error

    /*!
        \brief Saves album cover for track to file in PNG format.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \param filename - file name to store image.
        \throw std::runtime_error in case of any error.
    */
    virtual void savePNGCoverToFile(TrackDescription track_desc, int cover_width, int cover_height, const std::wstring& filename) const = 0; // throw std::runtime_error

    /*!
        \brief Registers notifier which will be called when specified event will occur.
               Client must call unRegisterListener() with gained ID to unsubscribe from notifications.
               It's important to call unRegisterListener() in listener dtor to avoid using of destroyed objects.
               Notification called from work thread of boost::asio::io_service object which is passed in AIMPManager ctor.
        \param event - type of event to notify.
        \param notifier - functor with signature 'void (EVENTS)'.
        \return listener ID that is used to unsubscribe from notifications.
    */
    virtual EventsListenerID registerListener(EventsListener listener) = 0;

    /*! Removes listener with specified ID.
        \param notifier_id - listener ID that returned by registerListener() method call.
    */
    virtual void unRegisterListener(EventsListenerID listener_id) = 0;

    virtual void onTick() = 0;
};

inline bool aimpStatusValid(int status)
    { return (AIMPManager::STATUS_FIRST < status && status < AIMPManager::STATUS_LAST); }

} // namespace AIMPPlayer

#endif // #ifndef AIMP_MANAGER_H
