// Copyright (c) 2013, Alexey Ivanov

#pragma once

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
        STATUS_ACTION_ON_END_OF_PLAYLIST, // [0,1,2] On the end of playlist "Jump to the next playlist"/"Repeat playlist"/"Do nothing
        STATUS_REPEAT_SINGLE_FILE_PLAYLISTS, // [0,1] off/on.
        STATUS_KBPS, // bitrate in kilobits/seconds.
        STATUS_KHZ, // sampling in Hertz, ex.: 44100.
        STATUS_MODE,
        STATUS_RADIO_CAPTURE, // [0,1] off/on.
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
        EVENT_RADIO_CAPTURE,
        EVENT_AIMP_QUIT, // supported by AIMP 3+.
        EVENTS_COUNT // not an event
    };

    enum PLAYLIST_ENTRY_SOURCE_TYPE {
        SOURCE_TYPE_FILE,
        SOURCE_TYPE_RADIO
    };

    typedef boost::function<void (EVENTS)> EventsListener;
    typedef std::size_t EventsListenerID;

    virtual ~AIMPManager() {}

    //! Starts playback.
    virtual void startPlayback() = 0;

    /*!
        \brief Starts playback of specified track.
        \param track_desc track descriptor, see TrackDescription struct declaration for details.
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

    //! \return current playing playlist. Can return 0 on from AIMP3 build 3.00.985 if there player is stopped.
    virtual PlaylistID getPlayingPlaylist() const = 0;

    //! \return current playing track. Can return 0 on from AIMP3 build 3.00.985 if there player is stopped.
    virtual PlaylistEntryID getPlayingEntry() const = 0; // throws std::runtime_error

    //! \return descriptor of playing track.
    virtual TrackDescription getPlayingTrack() const = 0;

    /*
        \return absolute playlist id.
                In case usual playlist id returns id itself.
                In case special playlist ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
    */
    virtual PlaylistID getAbsolutePlaylistID(PlaylistID id) const = 0;

    /*
        \return absolute entry id.
                In case usual entry id returns id itself.
                In case special entry ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
        \throw std::runtime_error if entry does not exist (for example, no playing entry when using ID = -1).
    */
    virtual PlaylistEntryID getAbsoluteEntryID(PlaylistEntryID id) const = 0; // throws std::runtime_error

    /*
        \return absolute descriptor of track.
                In case usual playlist/playlistentry ids returns track_desc itself.
                In case special playlist/playlistentry ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
        \throw std::runtime_error if track does not exist (for example, no playing track when using ID = -1).
    */
    virtual TrackDescription getAbsoluteTrackDesc(TrackDescription track_desc) const = 0; // throws std::runtime_error
    
    /*
        \return crc32 of playlist.
    */
    virtual crc32_t getPlaylistCRC32(PlaylistID playlist_id) const = 0; // throws std::runtime_error

    /*
        \return source type of the specified track.
    */
    virtual PLAYLIST_ENTRY_SOURCE_TYPE getTrackSourceType(TrackDescription track_desc) const = 0; // throws std::runtime_error

    //! \return ID of playback state.
    virtual PLAYBACK_STATE getPlaybackState() const = 0;

    /*!
        \brief Returns formatted entry descrition string. Acts like printf() analog, see details below.
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
            %R - artist
            %S - filesize
            %T - title
            %Y - date
            %M - rating
            %IF(A, B, C) - if A is empty use C else use B.
        Example: format_string = "%a - %T", result = "artist - title".
        </PRE>
        \return formatted string for entry.
    */
    virtual std::wstring getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const = 0; // throw std::invalid_argument

    /*
        Returns true if there is album cover is located in separate file (not embedded into track metadata) and it is available.
        argument path will be set if it is not null_ptr and album cover file is available.
    */
    virtual bool isCoverImageFileExist(TrackDescription track_desc, boost::filesystem::wpath* path = nullptr) const = 0; // throw std::runtime_error

    /*!
        \brief Saves album cover for track to file in format determined by filename extention.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param filename - file name to store image.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \throw std::runtime_error in case of any error.
    */
    virtual void saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width = 0, int cover_height = 0) const = 0; // throw std::runtime_error

    /*
        Returns track rating.
        rating value is in range [0-5]. Zero value means rating is not set.
    */
    virtual int trackRating(TrackDescription track_desc) const = 0; // throws std::runtime_error

    virtual void addFileToPlaylist(const boost::filesystem::wpath& path, PlaylistID playlist_id) = 0; // throws std::runtime_error
    
    virtual void addURLToPlaylist(const std::string& url, PlaylistID playlist_id) = 0; // throws std::runtime_error

    virtual PlaylistID createPlaylist(const std::wstring& title) = 0;

    virtual void removeTrack(TrackDescription track_desc, bool physically = false) = 0; // throws std::runtime_error

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
