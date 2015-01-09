// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "manager.h"
#include "aimp3.60_sdk/aimp3_60_sdk.h"
#include "aimp3.60_sdk/Helpers/typedefs.h"
#include "utils/util.h"
#include "playlist_queue.h"
#include "playlist_entry_rating.h"
#include "playlist_update_manager.h"
#include "player_supported_formats_getter.h"

namespace AimpRpcMethods {
    class EmulationOfWebCtlPlugin;
}

namespace AIMPPlayer
{

/*!
    \brief Extends AIMPManager with new functionality introduced in AIMP 3.60.
*/
class AIMPManager36 : public AIMPManager,
                      public IPlaylistQueueManager,
                      public IPlaylistEntryRatingManager,
                      public IPlaylistUpdateManager,
                      public IPlayerSupportedFormatsGetter
{
public:

    AIMPManager36(boost::intrusive_ptr<AIMP36SDK::IAIMPCore> aimp36_core, boost::asio::io_service& io_service); // throws std::runtime_error

    virtual ~AIMPManager36();

    //! Starts playback.
    virtual void startPlayback();

    /*!
        \brief Starts playback of specified track.
        \param track_desc track descriptor, see TrackDescription struct declaration for details.
        \throw std::runtime_error if track does not exist.
    */
    virtual void startPlayback(TrackDescription track_desc); // throws std::runtime_error

    //! Stops playback.
    virtual void stopPlayback();

    //! \return AIMP application version.
    virtual std::string getAIMPVersion() const;

    //! Sets pause.
    virtual void pausePlayback();

    //! Plays next track.
    virtual void playNextTrack();

    //! Plays previous track.
    virtual void playPreviousTrack();

    //! Gets value for specified status.
    virtual StatusValue getStatus(STATUS status) const;

    /*!
        \brief Sets AIMP status to specified value. See enum STATUS to know about available statuses and ranges of their values.
        \param status - status to set.
        \param value - depends on status.
        \throw std::runtime_error if status was not set successfully.
    */
    virtual void setStatus(STATUS status, StatusValue value); // throws std::runtime_error

    /*!
        \brief Enqueues specified track for playing.
        \param track_desc - track descriptor.
        \param insert_at_queue_beginning - flag, must be set to insert track at queue beginning, otherwise track will be set to end of queue.
        \throw std::runtime_error if track does not exist.
    */
    virtual void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning); // throws std::runtime_error

    /*!
        \brief Removes track from AIMP play queue.
        \param track_desc - track descriptor.
        \throw std::runtime_error if track does not exist.
    */
    virtual void removeEntryFromPlayQueue(TrackDescription track_desc); // throws std::runtime_error

    // IPlaylistQueueManager methods begin
    virtual void reloadQueuedEntries(); // throws std::runtime_error

    virtual void moveQueueEntry(TrackDescription track_desc, int new_queue_index); // throws std::runtime_error

    virtual void moveQueueEntry(int old_queue_index, int new_queue_index); // throws std::runtime_error
    // IPlaylistQueueManager methods end

    //! \return current playing playlist. Can return 0 on from AIMP3 build 3.00.985 if there player is stopped.
    virtual PlaylistID getPlayingPlaylist() const;

    //! \return current playing track. Can return 0 on from AIMP3 build 3.00.985 if there player is stopped.
    virtual PlaylistEntryID getPlayingEntry() const; // throws std::runtime_error

    //! \return descriptor of playing track.
    virtual TrackDescription getPlayingTrack() const;

    /*
        \return absolute playlist id.
                In case usual playlist id returns id itself.
                In case special playlist ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
    */
    virtual PlaylistID getAbsolutePlaylistID(PlaylistID id) const;

    /*
        \return absolute entry id.
                In case usual entry id returns id itself.
                In case special entry ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
        \throw std::runtime_error if entry does not exist (for example, no playing entry when using ID = -1).
    */
    virtual PlaylistEntryID getAbsoluteEntryID(PlaylistEntryID id) const; // throws std::runtime_error

    /*
        \return absolute descriptor of track.
                In case usual playlist/playlistentry ids returns track_desc itself.
                In case special playlist/playlistentry ids returns it's usual ids.
                Special IDs:
                    - currently playing: -1
        \throw std::runtime_error if track does not exist (for example, no playing track when using ID = -1).
    */
    virtual TrackDescription getAbsoluteTrackDesc(TrackDescription track_desc) const; // throws std::runtime_error
    
    /*
        \return crc32 of playlist.
    */
    virtual crc32_t getPlaylistCRC32(PlaylistID playlist_id) const; // throws std::runtime_error

    /*
        \return source type of the specified track.
    */
    virtual PLAYLIST_ENTRY_SOURCE_TYPE getTrackSourceType(TrackDescription track_desc) const; // throws std::runtime_error

    //! \return ID of playback state.
    virtual PLAYBACK_STATE getPlaybackState() const;

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
    virtual std::wstring getFormattedEntryTitle(TrackDescription track_desc, const std::string& format_string_utf8) const; // throw std::invalid_argument

    virtual std::wstring getEntryFilename(TrackDescription track_desc) const; // throw std::invalid_argument

    /*
        Returns true if there is album cover is located in separate file (not embedded into track metadata) and it is available.
        argument path will be set if it is not null_ptr and album cover file is available.
    */
    virtual bool isCoverImageFileExist(TrackDescription track_desc, boost::filesystem::wpath* path = nullptr) const; // throw std::runtime_error

    bool getCoverImageContainter(TrackDescription track_desc, boost::intrusive_ptr<AIMP36SDK::IAIMPImageContainer>* container = nullptr, boost::intrusive_ptr<AIMP36SDK::IAIMPImage>* image = nullptr) const; // throw std::runtime_error

    /*!
        \brief Saves album cover for track to file in format determined by filename extention.
               Size is determined by cover_width and cover_height arguments. Pass zeros to get full size cover.
        \param track_desc - track descriptor.
        \param filename - file name to store image.
        \param cover_width - cover width. If zero, width will be calculated from cover height(if it is non-zero), or original width will be used(if cover height is zero).
        \param cover_height - cover height. If zero, height will be calculated from cover width(if it is non-zero), or original height will be used(if cover width is zero).
        \throw std::runtime_error in case of any error.
    */
    virtual void saveCoverToFile(TrackDescription track_desc, const std::wstring& filename, int cover_width = 0, int cover_height = 0) const; // throw std::runtime_error

    /*
        Returns track rating.
        rating value is in range [0-5]. Zero value means rating is not set.
    */
    virtual double trackRating(TrackDescription track_desc) const; // throws std::runtime_error

    virtual void addFileToPlaylist(const boost::filesystem::wpath& path, PlaylistID playlist_id); // throws std::runtime_error
    
    virtual void addURLToPlaylist(const std::string& url, PlaylistID playlist_id); // throws std::runtime_error

    virtual PlaylistID createPlaylist(const std::wstring& title);

    virtual void removeTrack(TrackDescription track_desc, bool physically = false); // throws std::runtime_error

    /*!
        \brief Registers notifier which will be called when specified event will occur.
               Client must call unRegisterListener() with gained ID to unsubscribe from notifications.
               It's important to call unRegisterListener() in listener dtor to avoid using of destroyed objects.
               Notification called from work thread of boost::asio::io_service object which is passed in AIMPManager ctor.
        \param event - type of event to notify.
        \param notifier - functor with signature 'void (EVENTS)'.
        \return listener ID that is used to unsubscribe from notifications.
    */
    virtual EventsListenerID registerListener(EventsListener listener);

    /*! Removes listener with specified ID.
        \param notifier_id - listener ID that returned by registerListener() method call.
    */
    virtual void unRegisterListener(EventsListenerID listener_id);

    virtual void onTick();

    // IPlaylistEntryRatingManager method.
    virtual void trackRating(TrackDescription track_desc, double rating); // throw std::runtime_error

    virtual void playlistActivated(AIMP36SDK::IAIMPPlaylist* playlist);
    virtual void playlistAdded(AIMP36SDK::IAIMPPlaylist* playlist);
	virtual void playlistRemoved(AIMP36SDK::IAIMPPlaylist* playlist);
    virtual void playlistChanged(AIMP36SDK::IAIMPPlaylist* playlist, DWORD flags);

    // IPlaylistUpdateManager methods.
    virtual void lockPlaylist(PlaylistID playlist_id); // throws std::runtime_error
    virtual void unlockPlaylist(PlaylistID playlist_id); // throws std::runtime_error

    // IPlayerSupportedFormatsGetter method.
    virtual std::wstring supportedTrackExtentions(); // throws std::runtime_error

    sqlite3* playlists_db()
        { return playlists_db_; }
    sqlite3* playlists_db() const
        { return playlists_db_; }

    // Returns nullptr if item does not exist.
    AIMP36SDK::IAIMPPlaylistItem_ptr getPlaylistItem(PlaylistEntryID id) const;
    AIMP36SDK::IAIMPPlaylistItem_ptr getPlaylistItem(PlaylistEntryID id);

    AIMP36SDK::IAIMPPlaylist_ptr getPlaylist(PlaylistID id) const;
    AIMP36SDK::IAIMPPlaylist_ptr getPlaylist(PlaylistID id);

    void onAimpCoreMessage(DWORD AMessage, int AParam1, void* AParam2, HRESULT* AResult);

protected:
    
    sqlite3* playlists_db_;

private:
    
    void initializeAIMPObjects();
    
    void initPlaylistDB();
    void shutdownPlaylistDB();
    void deletePlaylistEntriesFromPlaylistDB(PlaylistID playlist_id);
    void deletePlaylistFromPlaylistDB(PlaylistID playlist_id);
    void updatePlaylistCrcInDB(PlaylistID playlist_id, crc32_t crc32); // throws std::runtime_error
    PlaylistCRC32& getPlaylistCRC32Object(PlaylistID playlist_id) const; // throws std::runtime_error

    // Returns -1 if handle not found in playlists list.
    int getPlaylistIndexByHandle(AIMP36SDK::IAIMPPlaylist* playlist);
    void loadPlaylist(AIMP36SDK::IAIMPPlaylist* playlist, int playlist_index);
    void loadEntries(AIMP36SDK::IAIMPPlaylist* playlist); // throws std::runtime_error
    void handlePlaylistChange(AIMP36SDK::IAIMPPlaylist* playlist, DWORD flags);
    void handlePlaylistUpdateTimer(AIMP36SDK::IAIMPPlaylist_ptr playlist, const boost::system::error_code& e);

    std::auto_ptr<ImageUtils::AIMPCoverImage> getCoverImage(boost::intrusive_ptr<AIMP36SDK::IAIMPImage> image, int cover_width, int cover_height) const;

    TrackDescription getTrackDescOfQueuedEntry(AIMP36SDK::IAIMPPlaylistItem* item) const; // throws std::runtime_error;
    void deleteQueuedEntriesFromPlaylistDB();

    void notifyAllExternalListeners(EVENTS event) const;
    // types for notifications of external event listeners.
    typedef std::map<EventsListenerID, EventsListener> EventListeners;
    EventListeners external_listeners_; //!< map of all subscribed listeners.
    EventsListenerID next_listener_id_; //!< unique ID describes external listener.

    boost::intrusive_ptr<AIMP36SDK::IAIMPCore> aimp36_core_;
    boost::intrusive_ptr<AIMP36SDK::IAIMPServicePlaylistManager> aimp_service_playlist_manager_;
    boost::intrusive_ptr<AIMP36SDK::IAIMPPlaylistQueue> aimp_playlist_queue_;
    
    boost::intrusive_ptr<AIMP36SDK::IAIMPServicePlayer> aimp_service_player_;
    boost::intrusive_ptr<AIMP36SDK::IAIMPServiceMessageDispatcher> aimp_service_message_dispatcher_;
    boost::intrusive_ptr<AIMP36SDK::IAIMPMessageHook> aimp_message_hook_;
    boost::intrusive_ptr<AIMP36SDK::IAIMPServiceAlbumArt> aimp_service_album_art_;

    class AIMPPlaylistListener;
    typedef boost::intrusive_ptr<AIMPPlaylistListener> AIMPPlaylistListener_ptr;
    typedef std::vector<AIMP36SDK::IAIMPPlaylistItem_ptr> PlaylistItems;

    struct PlaylistHelper {
        AIMP36SDK::IAIMPPlaylist_ptr playlist_;
        mutable PlaylistCRC32 crc32_;
        AIMPPlaylistListener_ptr listener_;
        PlaylistItems items_; // used for 1) addref each item to have persistent ID per item lifetime in playlist; 2) validation of external playlist item ID.


        struct PlaylistChanged {
            AIMPManager36* aimp36_manager_;
            static const boost::int32_t MIN_TIME_BETWEEN_PLAYLIST_CONTENT_UPDATES_MS = 1000;
            boost::posix_time::ptime last_time_;
            boost::shared_ptr<boost::asio::deadline_timer> playlist_changed_timer_;
            DWORD flags;

            PlaylistChanged(AIMPManager36* aimp36_manager)
                :
                aimp36_manager_(aimp36_manager),
                last_time_(boost::posix_time::microsec_clock::universal_time()),
                playlist_changed_timer_(new boost::asio::deadline_timer(aimp36_manager->io_service_)),
                flags(0)
            {}

        } playlist_changed_;

        PlaylistHelper(AIMP36SDK::IAIMPPlaylist_ptr playlist, AIMPManager36* aimp36_manager);
        ~PlaylistHelper();

        bool trySchedulePlaylistContentUpdate(DWORD flags);
    };

    typedef std::vector<PlaylistHelper> PlaylistHelpers;
    mutable PlaylistHelpers playlist_helpers_;
    PlaylistHelper& getPlaylistHelper(AIMP36SDK::IAIMPPlaylist* playlist); // throws std::runtime_error

    boost::asio::io_service& io_service_;

    // This class was made friend only for easy emulate web ctl plugin behavior. Remove when possible.
    friend class AimpRpcMethods::EmulationOfWebCtlPlugin;
};

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);

template<>
PlaylistID cast(AIMP36SDK::IAIMPPlaylist* playlist);

template<>
AIMP36SDK::IAIMPPlaylist* cast(PlaylistID id);

PlaylistEntryID castToPlaylistEntryID (AIMP36SDK::IAIMPPlaylistItem* item);

AIMP36SDK::IAIMPPlaylistItem* castToPlaylistItem(PlaylistEntryID id);

} // namespace AIMPPlayer
