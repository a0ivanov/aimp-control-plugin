// Copyright (c) 2013, Alexey Ivanov
/*! 
    \file
    \mainpage AIMP Control RPC functionality
*/
#pragma once

#include "aimp/manager.h"
#include "method.h"
#include "value.h"
#include "utils.h"
#include "utils/sqlite_util.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std.hpp>
#include <boost/bind.hpp>

#include "sqlite/sqlite.h"

namespace MultiUserMode { class MultiUserModeManager; }

namespace Rpc { class DelayedResponseSender; }

/*! contains RPC methods definitions.

    #ERROR_CODES \internal This must be mentioned to proper generation links to values of this enum \endinternal
*/
namespace AimpRpcMethods
{

/*!
    \page ids_info Playlist/track identifiers
    \section special_ids_sec Special IDs
    ID -1 is used as currently playing playlist/track.
    \section misc_sec Miscellaneous
    On AIMP3+ playlist ID is not needed because of track ID is unique there.
*/

/*!
    \page non_rpc_features Other features
    \section track_download_sec Downloading track
        Use GET request to URI /downloadTrack/playlist_id/\<playlst_id\>/track_id/\<track_id\>
    \section track_upload_sec Uploading track
        Use POST request with multipart form data content to URI /uploadTrack/playlist_id/\<playlst_id\><BR>
        Multiple file upload in one request is supported.<BR>
        Files will be stored at "%TMP%\Control plugin" directory.<BR>
        Internet radio URL adding to playlist is also supported by using text input field type. But URL can be added in more convenient way by AddURLToPlaylist.
*/

/*!
    \brief Error codes which RPC functions return.
    \internal Use codes in range [11-1000] for RPC methods errors. Rpc server uses range [1-10] for its errors. \endinternal
*/
enum ERROR_CODES { 
                   WRONG_ARGUMENT = 11, /*!< arguments passed in function can't be processed. Reasons: missing required arg, wrong type & etc. */
                   PLAYBACK_FAILED = 12, /*!< Track does not exist or track source is not available. */ 
                   SHUFFLE_MODE_SET_FAILED = 13, /*!< can't update shuffle mode. */
                   REPEAT_MODE_SET_FAILED = 14, /*!< can't update repeat mode. */
                   VOLUME_OUT_OF_RANGE = 15, /*!< volume level value is out of range [0, 100]. */
                   VOLUME_SET_FAILED = 16, /*!< can't update volume level. */
                   MUTE_SET_FAILED = 17, /*!< can't update mute mode. */
                   ENQUEUE_TRACK_FAILED = 18, /*!< can't add track to queue. */
                   DEQUEUE_TRACK_FAILED = 19, /*!< can't remove track from queue. */
                   PLAYLIST_NOT_FOUND = 20, /*!< specified playlist does not exist. */
                   TRACK_NOT_FOUND = 21, /*!< specified track does not exist. */
                   ALBUM_COVER_LOAD_FAILED = 22, /*!< can't get album cover. Possible reason: FreeImage.dll or FreeImagePlus.dll is not available for load by AIMP player. */
                   RATING_SET_FAILED = 23, /*!< can't set rating. */
                   STATUS_SET_FAILED = 24, /*!< can't set status. */
                   RADIO_CAPTURE_MODE_SET_FAILED = 25, /*!< can't update radio capture mode. Possible reason: currenly playing track source is not radio, but usual file. */
                   MOVE_TRACK_IN_QUEUE_FAILED = 26, /*!< can't update position of track in queue. Possible reason: position index is out of range. */
                   ADD_URL_TO_PLAYLIST_FAILED = 27, /*!< can't add url to playlist. Possible reason: playlist was not found. */
                   REMOVE_TRACK_FAILED = 28, /*!< can't remove track from playlist. Possible reason: track was not found. */
                   REMOVE_TRACK_PHYSICAL_DELETION_DISABLED = 29 /*!< can't remove track physically. Reason: user has disabled it in plugin settings. */
};

using namespace AIMPPlayer;
using namespace MultiUserMode;

/*
    Base class for all AIMP RPC methods.
    Contains reference to AIMPManager.
*/
class AIMPRPCMethod : public Rpc::Method
{
public:

    AIMPRPCMethod(  const std::string& name,
                    AIMPManager& aimp_manager,
                    Rpc::RequestHandler& rpc_request_handler
                 )
        :
        Rpc::Method(name),
        aimp_manager_(aimp_manager),
        rpc_request_handler_(rpc_request_handler)
    {}

    virtual ~AIMPRPCMethod() {}

protected:

    TrackDescription getTrackDesc(const Rpc::Value& params) const; // throws Rpc::Exception

    AIMPManager& aimp_manager_;
    Rpc::RequestHandler& rpc_request_handler_;

private:

    AIMPRPCMethod();
};

/*! 
    \brief Starts playback.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More"
    \remark If no params is specified, starts playback of current track in current playlist.
    \return object which describes:
        - success:<BR>
            Example: \code "result":{"playback_state":"playing","playlist_id":37036512,"track_id":0} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::PLAYBACK_FAILED

            Example: \code {"error":{"code":12,"message":"Playback specified track failed. Track does not exist or track's source is not available."}} \endcode
*/
class Play : public AIMPRPCMethod
{
public:
    Play(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Play", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "There are two ways to use 'play' function: "
               "Play() plays current track in current playlist. "
               "play_track_in_playlist(int track_id, int playlist_id) plays specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Pauses playback.
    \return object which describes current playback state.
            Example: \code{ "playback_state" : "paused" }\endcode
*/
class Pause : public AIMPRPCMethod
{
public:
    Pause(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Pause", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "Pause() pauses playback.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Stops playback.
    \return object which describes current playback state.
            Example:\code{ "playback_state" : "stopped" }\endcode
*/
class Stop : public AIMPRPCMethod
{
public:
    Stop(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Stop", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "Stop() stops playback.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Plays previous track.
    \return object which describes current track.
            Example:\code{ "track_id" : 0, "playlist_id" : 2136855104 }\endcode
*/
class PlayPrevious : public AIMPRPCMethod
{
public:
    PlayPrevious(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("PlayPrevious", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "PlayPrevious() plays previous track.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Plays next track.
    \return object which describes current track.
            Example:\code{ "track_id" : 0, "playlist_id" : 2136855104 }\endcode
*/
class PlayNext : public AIMPRPCMethod
{
public:
    PlayNext(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("PlayNext", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "PlayNext() plays next track.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Gets/sets AIMP status.
    \param status_id - int: ID of required status. The following IDs are available(status ID, status value range):
      - VOLUME      = 1,  [0,100] in percents: 0 = 0%, 100 = 100%
      - BALANCE     = 2,  [0,100]: 0 left only, 50 left=right, 100 right.
      - SPEED       = 3,  [0,100] in percents: 0 = 50%, 50 = 100%, 100 = 150%.
      - Player      = 4,  [0,1,2] stop, play, pause
      - MUTE        = 5,  [0,1] off/on
      - REVERB      = 6,  [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%
      - ECHO        = 7,  [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%
      - CHORUS      = 8,  [0,100]: 0 disabled, 10=1%, 49=54%, 100=99%
      - Flanger     = 9,  [0,100]: 0 disabled, 6=2%, 51=46%, 100=99%
      - EQ_STS      = 10, [0,1] off/on
      - EQ_SLDR01   = 11, EQ_SLDRXX [0,100]: 0 = -15db, 50 = 0db, 100 = 15db
      - EQ_SLDR02   = 12,
      - EQ_SLDR03   = 13,
      - EQ_SLDR04   = 14,
      - EQ_SLDR05   = 15,
      - EQ_SLDR06   = 16,
      - EQ_SLDR07   = 17,
      - EQ_SLDR08   = 18,
      - EQ_SLDR09   = 19,
      - EQ_SLDR10   = 20,
      - EQ_SLDR11   = 21,
      - EQ_SLDR12   = 22,
      - EQ_SLDR13   = 23,
      - EQ_SLDR14   = 24,
      - EQ_SLDR15   = 25,
      - EQ_SLDR16   = 26,
      - EQ_SLDR17   = 27,
      - EQ_SLDR18   = 28,
      - REPEAT      = 29,  [0,1] off/on
      - ON_STOP     = 30,
      - POS         = 31,  [0,TRACK Length) in seconds
      - LENGTH      = 32,  track length in seconds
      - REPEATPLS   = 33,  [0,1,2] "Jump to the next playlist"/"Repeat playlist"/"Stand by" (AIMP2 access: Settings->Playlist->Automatics->On the end of playlist->Action)
      - REP_PLS_1   = 34,  [0,1] Enable/Disable (AIMP2 access: Settings->Playlist->Automatics->"Disable repeat when only one file in playlist")
      - KBPS        = 35,  bitrate in kilobits/seconds.
      - KHZ         = 36,  sampling in Hertz, ex.: 44100
      - MODE        = 37,  don't understand
      - RADIO_CAPTURE = 38, [0,1] off/on
      - STREAM_TYPE = 39,  0 mp3, 1 ?, 2 internet radio
      - TIMER       = 40,
      - SHUFFLE     = 41,   [0,1] off/on
    \param value - int, optional: new status value. Value range depends on status ID. If value is not specified, function returns current status.
    \return object which describes current status value. Range and meaning of value depends on status. See AIMP2 SDK for details. On AIMP3 all values are mapped to AIMP2 ranges.
            Example:\code{ "value" : 0 }\endcode
*/
class Status : public AIMPRPCMethod
{
public:
    Status(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Status", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int Status(int status_id, int value) sets specified status of player to specified value. "
               "If status_value is not specified returns current requested status.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    bool statusGetSetSupported(AIMPManager::STATUS status) const;
};

/*! 
    \brief Gets/sets shuffle playback mode.
    \param shuffle_on - boolean, optional.
    \return object which describes current shuffle mode.
            Example:\code{ "shuffle_mode_on" : false }\endcode
    \remark it's equivalent to Status function called with status ID SHUFFLE.
*/
class ShufflePlaybackMode : public AIMPRPCMethod
{
public:
    ShufflePlaybackMode(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("ShufflePlaybackMode", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool ShufflePlaybackMode(bool shuffle_on) activates or deactivates shuffle playback mode. "
               "If no arguments are passed returns current state of shuffle mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Gets/sets repeat playback mode.
    \param repeat_on - boolean, optional.
    \return object which describes current repeat mode.
            Example:\code{ "repeat_mode_on" : false }\endcode
    \remark it's equivalent to Status function called with status ID REPEAT.
*/
class RepeatPlaybackMode : public AIMPRPCMethod
{
public:
    RepeatPlaybackMode(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("RepeatPlaybackMode", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool RepeatPlaybackMode(bool repeat_on) activates or deactivates repeat playlist playback mode. "
               "If no arguments are passed returns current state of repeat mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Gets/sets volume level.
    \param level - int, optional. Volume level in percents. Must be in range [0, 100].
    \return object which describes current volume level.
            Example:\code{ "volume" : 100 }\endcode
    \remark it's equivalent to Status function called with status ID VOLUME.
*/
class VolumeLevel : public AIMPRPCMethod
{
public:
    VolumeLevel(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("VolumeLevel", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int VolumeLevel(int level) sets volume level(in percents). By default returns current volume level.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Gets/sets mute mode.
    \param mute_on - boolean, optional.
    \return object which describes current mute mode status.
            Example:\code{ "mute_on" : false }\endcode
    \remark it's equivalent to Status function called with status ID MUTE.
*/
class Mute : public AIMPRPCMethod
{
public:
    Mute(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Mute", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool Mute(bool mute_on) activates or deactivates mute mode. "
               "If no arguments are passed returns current state of repeat mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Gets/sets radio capture mode.
    \param radio_capture_on - boolean, optional.
    \return object which describes current shuffle mode.
            Example:\code{ "radio_capture_on" : false }\endcode
    \remark it's equivalent to Status function called with status ID RADIO_CAPTURE.
*/
class RadioCaptureMode : public AIMPRPCMethod
{
public:
    RadioCaptureMode(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("RadioCaptureMode", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool RadioCaptureMode(bool radio_capture_on) activates or deactivates radio capture mode. "
               "If no arguments are passed returns current state of radio capture mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*!
    \brief Returns formatted string for specified track.

           Acts like printf() analog, see details below.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More".
    \param format_string - string, can contain following format arguments:
    \verbatim
    %A - album
    %a - artist
    %B - bitrate
    %C - channels count
    %G - genre
    %H - sample rate
    %L - duration
    %S - filesize
    %T - title
    %Y - date
    %M - rating
    %IF(A,B,C) - if A is empty use C else use B.
    \endverbatim

    \return formatted_string - string, formatted string for entry.
            Example:\code{"formatted_string":".:: 03:02 :: Stereophonics - A Thousand Trees :: MP3 :: 44 kHz, 320 kbps, 6.98 Mb ::."}\endcode
            when format_string is \code".:: %L :: %IF(%a,%a - %T,%T) :: %E :: %H, %B, %S ::."\endcode
    \remark You need to escape \verbatim characters %,) with '%' char. Example: "%IF(%a , %a %%%,%) %T, %T)" will be formatted as "Stereophonics %,) A Thousand Trees" \endverbatim
*/
class GetFormattedEntryTitle : public AIMPRPCMethod
{
public:
    GetFormattedEntryTitle(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("GetFormattedEntryTitle", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return  "\return formatted string for specified track in specified playlist."
                "Param params.track_id - track ID."
                "Param params.playlist_id - playlist ID."
                "Param params.format_string - format string. There are following format arguments:"
                "    %A - album"
                "    %a - artist"
                "    %B - bitrate"
                "    %C - channels count"
                "    %G - genre"
                "    %H - sample rate"
                "    %L - duration"
                "    %S - filesize"
                "    %T - title"
                "    %Y - date"
                "    %M - rating"
                "Example: format_string = '%a - %T', result = 'artist - title'."
            "Result - is object with member 'formatted_string', string."
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
} ;
typedef GetFormattedEntryTitle get_formatted_entry_title;

/*! 
    \brief Returns info about track.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More".
    \return object which describes specified track.
            Example:\code{"album":"L.A. Noire Official Soundtrack","artist":"Andrew Hale and Simon Hale","bitrate":320,"channels_count":2,"date":"2011",
                          "duration":198557,"filesize":7944619,"genre":"Soundtrack","id":52841544,"playlist_id":40396992,
                          "rating":0,"samplerate":44100,"title":"New Beginning (Part 3)"}\endcode
*/
class GetPlaylistEntryInfo : public AIMPRPCMethod
{
public:
    GetPlaylistEntryInfo(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("GetPlaylistEntryInfo", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "GetPlaylistEntryInfo(int playlist_id, int track_id) returns info about track represented as struct with following fields:"
               " 'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'."
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Enqueues track for playing.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More"
    \param insert_at_queue_beginning - boolean, optional, default is false. If true enqueue at the beginning of queue.
    \return object which describes:
        - success:<BR>
            Example: \code "result":{} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::ENQUEUE_TRACK_FAILED

            Example: \code {"error":{"code":18,"message":"Enqueue track failed. Reason: internal AIMP error."}} \endcode

*/
class EnqueueTrack : public AIMPRPCMethod
{
public:
    EnqueueTrack(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("EnqueueTrack", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "EnqueueTrack(int track_id, int playlist_id, bool insert_at_queue_beginning = false) "
               "enqueues for playing specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Removes track from AIMP play queue.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More"
    \return object which describes:
        - success:<BR>
            Example: \code "result":{} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::DEQUEUE_TRACK_FAILED

            Example: \code {"error":{"code":19,"message":"Removing track from play queue failed. Reason: internal AIMP error."}} \endcode
*/
class RemoveTrackFromPlayQueue : public AIMPRPCMethod
{
public:
    RemoveTrackFromPlayQueue(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("RemoveTrackFromPlayQueue", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "RemoveTrackFromPlayQueue(int track_id, int playlist_id) "
               "removes from AIMP play queue specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Moves track to specified position in queue.
    \param track_id - int, optional. If specified param playlist_id is obligatory on AIMP2. Note: track should be already queued.  
    \param playlist_id - int, optional. If specified param track_id is obligatory. Note: track should be already queued.
    \param old_queue_index - int, optional. If specified params track_id and playlist_id are not needed.
    \param new_queue_index - int.
    \return object which describes:
        - success:<BR>
            Example: \code "result":{} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::MOVE_TRACK_IN_QUEUE_FAILED

            Example: \code {"error":{"code":26,"message":"Enqueue track failed. Reason: internal AIMP error."}} \endcode
*/
class QueueTrackMove : public AIMPRPCMethod
{
public:
    QueueTrackMove(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("QueueTrackMove", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Returns list of playlists.
    \param fields - array of strings, optional. List of fields that need to be filled.
           Available fields are:
            - id
            - title
            - duration
            - entries_count
            - size_of_entries
            - crc32

           If 'fields' param is not specified the function treats it equals "id, title".
    \return object which describes playlists.
            Example:\code{"id":2136854848,"title":"Default"},{"id":2136855104,"title":"stereophonics"}\endcode
*/
class GetPlaylists : public AIMPRPCMethod
{
public:

    GetPlaylists(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        :
        AIMPRPCMethod("GetPlaylists", aimp_manager, rpc_request_handler),
        playlist_fields_filler_("playlist")
    {
        using namespace RpcValueSetHelpers;
        using namespace RpcResultUtils;
        auto int_setter   = boost::bind( createSetter(&sqlite3_column_int),   _1, _2, _3 );
        auto int64_setter = boost::bind( createSetter(&sqlite3_column_int64), _1, _2, _3 );
        auto text_setter  = boost::bind( createSetter(&sqlite3_column_text),  _1, _2, _3 );
        boost::assign::insert(playlist_fields_filler_.setters_)
            ( getStringFieldID(Playlist::ID),                           int_setter )
            ( getStringFieldID(Playlist::TITLE),                        text_setter )
            ( getStringFieldID(Playlist::DURATION),                     int64_setter )
            ( getStringFieldID(Playlist::ENTRIES_COUNT),                int_setter )
            ( getStringFieldID(Playlist::SIZE_OF_ALL_ENTRIES_IN_BYTES), int64_setter )
            ( getStringFieldID(Playlist::CRC32),                        int64_setter )
        ;
    }

    std::string help()
    {
        return "GetPlaylists(string playlist_fields[]) returns array of playlists. "
               "Playlist can have following fields: "
                   "'id', 'title', 'duration', 'entries_count', 'size_of_entries', 'crc32'."
               "Required fields passed in playlist_fields string array."
               "Each playlist in response is associative array with keys specified in playlist_fields[].";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    std::string getColumnsString() const;

    //! See HelperFillRpcFields class commentaries.
    RpcValueSetHelpers::HelperFillRpcFields<Playlist> playlist_fields_filler_;
};

namespace PlaylistEntries {
typedef std::set<std::string> SupportedFieldNames;
typedef std::vector<SupportedFieldNames::const_iterator> RequiredFieldNames;
}

struct PaginationInfo : boost::noncopyable {
    PaginationInfo(int entry_id, size_t id_field_index)
        : entry_id(entry_id),
          id_field_index(id_field_index),
          entries_on_page_(0),
          entry_index_in_current_representation_(-1)
    {}

    const int entry_id;
    const size_t id_field_index;
    size_t entries_on_page_;
    int entry_index_in_current_representation_; // index of entry in representation(concrete filtering and sorting) of playlist entries.
};

/*! 
    \brief Returns list of playlist entries.
    \param playlist_id - int. \ref special_ids_sec "More"
    \param format_string - string, optional. If specified entry will be presented as string, instead set of fields. Mutual exclusive with 'fields' param.
    \param fields - array of strings, optional. List of fields that need to be filled.
           If 'fields' param is not specified the function treats it equals "id, title".
           Mutual exclusive with 'format_string' param.
           Available fields are:
            - id
            - title
            - artist
            - album
            - date
            - genre
            - bitrate
            - duration
            - filesize
            - rating
    \param start_index - int, optional(Default is 0). Beginnig of required entries range.
    \param entries_count - int, optional(Default is counnt of available entries). Required count of entries.
    \param order_fields - array of field descriptions, optional. It is used to order entries by multiple fields.
           Each descriptor is object with members:
                - 'field' - field to order. Available fields are: 'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'.
                - 'dir' - order('asc' - ascending, 'desc' - descending)
    \param search_string - string, optional. Only those entries will be returned which have at least
                                             one occurence of 'search_string' value in one of entry string fields:
                                                - title
                                                - artist
                                                - album
                                                - data
                                                - genre

    \return object which describes playlist entries.
            Example:\code{"count_of_found_entries":1,"entries":[[1,"Looks Like Chaplin"]],"total_entries_count":3}\endcode
            If params were \code{"playlist_id": 2136855360, "search_string":"Like"}}\endcode
*/
class GetPlaylistEntries : public AIMPRPCMethod
{
public:
    GetPlaylistEntries(AIMPManager& aimp_manager,
                       Rpc::RequestHandler& rpc_request_handler
                       );

    std::string help()
    {
        return "get_playlist_entries(int playlist_id, string fields[], int start_index, int entries_count, struct order_fields[], string search_string, string format_string) "
               "returns struct with following members: "
               "    'count_of_found_entries' - (optional value. Defined if params.search_string is not empty) - count of entries "
                                               "which match params.search_string. See params.search_string param description for details. "
               "    'entries' - array of entries. Entry is object with members specified by params.fields. "
               ;//+ get_playlist_entries_templatemethod_->help();
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

    void activateEntryLocationDeterminationMode(PaginationInfo* pagination_info)
        { pagination_info_ = pagination_info; }
    void activateQueuedEntriesMode()
        { queued_entries_mode_ = true; }

private:

    void deactivateEntryLocationDeterminationMode()
        { pagination_info_ = nullptr; }
    bool entryLocationDeterminationMode() const
        { return pagination_info_ != nullptr; }

    bool queuedEntriesMode() const
        { return queued_entries_mode_; }
    void deactivateQueuedEntriesMode()
        { queued_entries_mode_ = false; }

    void addSpecialFieldsSupport();
    void removeSpecialFieldsSupport();

    // See HelperFillRpcFields class commentaries.
    RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry> entry_fields_filler_;
    void initEntriesFiller(const Rpc::Value& params);

    typedef std::vector<std::string> FieldNames;

    FieldNames fields_to_order_;
    std::string getOrderString(const Rpc::Value& params) const;

    FieldNames fields_to_filter_;

    typedef std::map<std::string, std::string> MapFieldnamesRPCToDB;
    MapFieldnamesRPCToDB fieldnames_rpc_to_db_;

    mutable Utilities::QueryArgSetters query_arg_setters_;

    std::string getLimitString(const Rpc::Value& params) const;
    std::string getWhereString(const Rpc::Value& params, const int playlist_id) const;
    std::string getColumnsString() const;
    size_t getTotalEntriesCount(sqlite3* playlists_db, const int playlist_id) const; // throws std::runtime_error

    const std::string kRQST_KEY_FORMAT_STRING,
                      kRQST_KEY_FIELDS;

    const std::string kRQST_KEY_START_INDEX,
                      kRQST_KEY_ENTRIES_COUNT;

    const std::string kRQST_KEY_FIELD,
                      kDESCENDING_ORDER_STRING,
                      kRQST_KEY_ORDER_DIRECTION,
                      kRQST_KEY_ORDER_FIELDS;

    const std::string kRQST_KEY_SEARCH_STRING;

    const std::string kRSLT_KEY_ENTRIES,
                      kRSLT_KEY_TOTAL_ENTRIES_COUNT,
                      kRSLT_KEY_COUNT_OF_FOUND_ENTRIES;

    const std::string kRQST_KEY_FIELD_PLAYLIST_ID,
                      kRSLT_KEY_FIELD_QUEUE_INDEX;

    PaginationInfo* pagination_info_;
    bool queued_entries_mode_;
};

/*! 
    \brief Returns page number and index of track on page.
    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More"
    \param start_index - the same as GetPlaylistEntries 's param.
    \param entries_count - the same as GetPlaylistEntries 's param.
    \param order_fields - the same as GetPlaylistEntries 's param.
    \param search_string - the same as GetPlaylistEntries 's param.

    \return object which describes entry location.
            Example:\code{"page_number":0,"track_index_on_page":0}\endcode
            If requested track exists but is not fetched with current request(for ex., does not contain search string), result will be \code{"page_number":-1,"track_index_on_page":-1}\endcode
*/
class GetEntryPositionInDataTable : public AIMPRPCMethod
{
public:

    // Note: we pass GetPlaylistEntries object by reference here, so we need to guarantee that it's lifetime is longer than lifetime of this object.
    GetEntryPositionInDataTable(AIMPManager& aimp_manager,
                                Rpc::RequestHandler& rpc_request_handler,
                                GetPlaylistEntries& getplaylistentries_method
                                );

    std::string help()
    {
        return "get_entry_position_in_datatable() requires the same args as get_playlist_entries() method + int track_id argument. "
               "Result is object with following members:"
		               "int page_number - if track is not found this value is -1. "
                       "int track_index_on_page - if track is not found this value is -1. "
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    GetPlaylistEntries& getplaylistentries_method_;
    const std::string kFIELD_ID;
};

/*! 
    \brief Returns count of entries in playlist.
    \param playlist_id - int. \ref special_ids_sec "More"
    \return count of entries.
            Example:\code0\endcode            
*/
class GetPlaylistEntriesCount : public AIMPRPCMethod
{
public:
    GetPlaylistEntriesCount(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("GetPlaylistEntriesCount", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int GetPlaylistEntriesCount(int playlist_id) returns count of entries in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Returns ordered list of queued entries.
           Params are the same as in GetPlaylistEntries except playlist_id and order_fields.
    \param format_string - string, optional. If specified entry will be presented as string, instead set of fields. Mutual exclusive with 'fields' param.
    \param fields - array of strings, optional. List of fields that need to be filled.
           Mutual exclusive with 'format_string' param.
           Available fields are:
            - playlist_id
            - id
            - queue_index
            - title
            - artist
            - album
            - date
            - genre
            - bitrate
            - duration
            - filesize
            - rating
           If 'fields' param is not specified the function treats it equals "id, title".
    \param start_index - int, optional(Default is 0). Beginnig of required entries range.
    \param entries_count - int, optional(Default is counnt of available entries). Required count of entries.
    \param search_string - string, optional. Only those entries will be returned which have at least
                                             one occurence of 'search_string' value in one of entry string fields:
                                                - title
                                                - artist
                                                - album
                                                - data
                                                - genre

    \return object which describes queued entries.
            Example:\code{"count_of_found_entries":1,"entries":[[45246368,0,0,"Main Theme","Andrew Hale","2011",184842]],"total_entries_count":1}\endcode
            If params were \code{"fields":["playlist_id","id","queue_index","title","artist","date","duration"]}\endcode
*/
class GetQueuedEntries : public AIMPRPCMethod
{
public:

    // Note: we pass GetPlaylistEntries object by reference here, so we need to guarantee that it's lifetime is longer than lifetime of this object.
    GetQueuedEntries(AIMPManager& aimp_manager,
                     Rpc::RequestHandler& rpc_request_handler,
                     GetPlaylistEntries& getplaylistentries_method
                     );

    std::string help()
    {
        return ""
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    GetPlaylistEntries& getplaylistentries_method_;
};

/*! 
    \brief Returns URI of album cover.
    \param playlist_id - int. \ref ids_info "More"
    \param track_id - int. \ref ids_info "More"
    \param cover_width - int, optional.
    \param cover_height - int, optional.
    
    \return URI of cover.
            Example:\code{"album_cover_uri":"tmp/cover_35_2136855104_0x0_99263.png"}\endcode 
    \remark Function is available only if FreeImage.dll and FreeImagePlus.dll are available for loading by AIMP.
*/
class GetCover : public AIMPRPCMethod
{
public:
    GetCover(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler, const boost::filesystem::wpath& document_root, const boost::filesystem::wpath& cover_directory, bool free_image_dll_is_available)
        :
        AIMPRPCMethod("GetCover", aimp_manager, rpc_request_handler),
        document_root_(document_root),
        free_image_dll_is_available_(free_image_dll_is_available),
        die_( rng_engine_, random_range_ ) // init generator by range[0, 9]
    {
        random_file_part_.resize(kRANDOM_FILENAME_PART_LENGTH);
        cover_directory_relative_ = cover_directory;
        prepare_cover_directory();
    }

    std::string help()
    {
        return "GetCover(int track_id, int playlist_id, int cover_width, int cover_height) "
               "returns URI of cover(size is determined by width and height arguments, pass zeros to get full size cover) "
               "of specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    /*
        Generate filename in format cover_playlistID_trackID_widthxheight_random. Ex: cover_2222222_01_100x100_45730.
    */
    std::wstring getTempFileNameForAlbumCover(TrackDescription track_desc, std::size_t width, std::size_t height);

    void prepare_cover_directory(); // throws runtime_error

    boost::filesystem::wpath document_root_,
                             cover_directory_relative_;

    boost::filesystem::wpath cover_directory() const
        { return (document_root_ / cover_directory_relative_).normalize(); }

    bool free_image_dll_is_available_;

    //  random utils
    typedef boost::variate_generator<boost::mt19937&, boost::uniform_int<> > RandomNumbersGenerator;
    static const int kRANDOM_FILENAME_PART_LENGTH = 5;
    boost::uniform_int<> random_range_;
    boost::mt19937 rng_engine_;
    RandomNumbersGenerator die_;
    std::wstring random_file_part_;

    class Cache {
    public:
        struct Entry {
            typedef std::list<std::wstring> Filenames;
            boost::shared_ptr<Filenames> filenames;
        };

        struct SearchResult {
            const Entry* entry;
            std::size_t uri_index;
            SearchResult(const Entry* entry = nullptr, std::size_t uri_index = 0) : entry(entry), uri_index(uri_index) {}
            operator bool() { return entry != nullptr; }
        };

        void cacheNew(TrackDescription track_desc, const boost::filesystem::wpath& album_cover_filename, const std::wstring& cover_uri_generic);
        void cacheBasedOnPreviousResult(TrackDescription track_desc, GetCover::Cache::SearchResult search_result);

        SearchResult isCoverCachedForCurrentTrack(TrackDescription track_desc, std::size_t width, std::size_t height) const;
        
        SearchResult isCoverCachedForAnotherTrack(const boost::filesystem::wpath& cover_path, std::size_t width, std::size_t height) const;

    private:
        SearchResult findInEntryBySize(const Entry& info, std::size_t width, std::size_t height) const;

        typedef std::map<TrackDescription, Entry> EntryMap;
        EntryMap entries_;

        typedef std::map<boost::filesystem::wpath, TrackDescription> PathTrackMap;
        PathTrackMap path_track_map_;
    };

    Cache cache_;
};

/*! 
    \brief Returns state of AIMP control panel.
    \return object which describes state.
            Example:\code{"mute_mode_on":false,"playback_state":"stopped","playlist_id":2136855104,"repeat_mode_on":false,"shuffle_mode_on":true,"track_id":25,"volume":45}\endcode            
*/
class GetPlayerControlPanelState : public AIMPRPCMethod
{
public:
    GetPlayerControlPanelState(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("GetPlayerControlPanelState", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "GetPlayerControlPanelState() returns current AIMP control panel state."
               "\return structure with following members:"
               "1) "
               // TODO: add description here
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*!
    \brief Notifies client about event.
    
    Comet technique implementation.
    This function returns result as soon as(not immediatelly) specified event occures.
    \param event - string. ID of event to subscribe.
            Supported events are:
                - 'play_state_change' - playback state change event (player switched to playing/paused/stopped state)
                  Response example:\code{"playback_state":"playing","track_length":269,"track_position":0}\endcode
                - 'current_track_change' - current track change event (player switched track)
                  Response example:\code{"playlist_id":2136855104,"track_id":84}\endcode
                - 'control_panel_state_change' - one of following aspects is changed:
                    - playback state
                    - mute
                    - shuffle
                    - repeat
                    - volume level
                    - radio capture
                    - AIMP application exit
                  Response is the same as GetPlayerControlPanelState function.
                  On AIMP app exit response also contains boolean field "aimp_app_is_exiting".
                  Response example:\code{"current_track_source_radio":true,"mute_mode_on":false,"playback_state":"playing","playlist_id":93533808,"radio_capture_mode_on":false,"repeat_mode_on":false,"shuffle_mode_on":false,"track_id":2,"volume":30}\endcode
                - 'playlists_content_change' - change of: 
                    - playlist's content
                    - playlist count
                  Response example:\code{"playlists_changed":true, "playlists":[{"crc32":-1169477297,"id":38609376},{"crc32":358339139,"id":38609520},{"crc32":-1895027311,"id":38609664}]}\endcode
                  Note: 'playlists' array contains all playlists.
*/
class SubscribeOnAIMPStateUpdateEvent : public AIMPRPCMethod
{
public:
    SubscribeOnAIMPStateUpdateEvent(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("SubscribeOnAIMPStateUpdateEvent", aimp_manager, rpc_request_handler),
          aimp_app_is_exiting_(false)
    {
        using namespace boost::assign;
        insert(event_types_)
            ("play_state_change",          PLAYBACK_STATE_CHANGE_EVENT)
            ("current_track_change",       CURRENT_TRACK_CHANGE_EVENT)
            ("control_panel_state_change", CONTROL_PANEL_STATE_CHANGE_EVENT)
            ("playlists_content_change",   PLAYLISTS_CONTENT_CHANGE_EVENT)
        ;

        aimp_events_listener_id_ = aimp_manager_.registerListener( boost::bind(&SubscribeOnAIMPStateUpdateEvent::aimpEventHandler,
                                                                               this,
                                                                               _1
                                                                               )
                                                                  );
    }

    virtual ~SubscribeOnAIMPStateUpdateEvent()
        { aimp_manager_.unRegisterListener(aimp_events_listener_id_); }

    std::string help()
    {
        return "SubscribeOnAIMPStateUpdateEvent(string event_id) returns AIMP state descriptor when event (specified by id) occures."
               "AIMP state descriptor is struct with members correspond to specified event type."
               "Supported events are:"
                   "1) 'play_state_change' - playback state change event (player switch to playing/paused/stopped state) and event when user forces change of track position."
                       "Response will contain following members:"
                           "'playback_state', string - playback state (playing, stopped, paused)"
                           "'track_position', int - current track position. Exist only if it has sense."
                           "'track_length', int - current track length. Exist only if it has sense."
                   "2) 'current_track_change' - current track change event (player switched track)"
                       "Response will contain following member:"
                           "1) 'playlist_id', int - playlist ID"
                           "2) 'track_id', int - track ID"
                   "3) 'control_panel_state_change' - one of following events:"
                           "playback state, mute, shuffle, repeat, volume level change, aimp app exit"
                       "Response is the same as get_control_panel_state() function."
                       "On aimp exit response also contains boolean field 'aimp_app_is_exiting'."
                   "4) 'playlists_content_change' - playlists content change"
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    //! types of events(AIMPManager state changes events), used for register/unregister notifiers.
    enum EVENTS { PLAYBACK_STATE_CHANGE_EVENT,
                  CURRENT_TRACK_CHANGE_EVENT,
                  CONTROL_PANEL_STATE_CHANGE_EVENT,
                  PLAYLISTS_CONTENT_CHANGE_EVENT
    };

    //! Converts AIMPManager::EVENTS to our EVENTS and calls sendNotifications() for them.
    void aimpEventHandler(AIMPManager::EVENTS event);

    //! Sends notification to all subscribers for specified event.
    void sendNotifications(EVENTS event_id);

    //! Formats result Rpc value according to specified event.
    void prepareResponse(EVENTS event_id, Rpc::Value& result) const;

    struct ResponseSenderDescriptor;
    //! Sends notification to concrete subscriber(delayed_response_sender) for specified event.
    void sendEventNotificationToSubscriber(EVENTS event_id,
                                           ResponseSenderDescriptor& response_sender_descriptor);

    /* Gets event from Rpc argument(string) in format of EVENTS. */
    EVENTS getEventFromRpcParams(const Rpc::Value& params) const;

    /* Map external string event id to EVENTS. */
    typedef std::map<std::string, EVENTS> EventTypesMap;
    EventTypesMap event_types_;

    AIMPManager::EventsListenerID aimp_events_listener_id_; // to receive AIMPManager events we subscribe on them by calling AIMPManager::registerListener().

    // link response sender with root_request.
    struct ResponseSenderDescriptor {
        Rpc::Value root_request;
        boost::shared_ptr<Rpc::DelayedResponseSender> sender;
        ResponseSenderDescriptor(const Rpc::Value& root_request,
                                 boost::shared_ptr<Rpc::DelayedResponseSender> sender)
            : root_request(root_request), sender(sender)
        {}
    };
    typedef std::multimap<EVENTS, ResponseSenderDescriptor> DelayedResponseSenderDescriptors;
    DelayedResponseSenderDescriptors delayed_response_sender_descriptors_;

    bool aimp_app_is_exiting_;
};

/*!
    \brief Sets track rating.

    On AIMP3: fully functional.<BR>
    On AIMP2: 
        Current implementation just saves path to track and rating in text file since AIMP SDK does not support rating change now.
        No checks are performed(ex. rating already exists for file and etc.) since it is temporarily stub.

    \param track_id - int. \ref ids_info "More"
    \param playlist_id - int. \ref ids_info "More"
    \param rating - int. Range [0-5], zero means rating is not set.
*/
class SetTrackRating : public AIMPRPCMethod
{
public:
    SetTrackRating(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler, const std::wstring& file_to_save_ratings)
        : AIMPRPCMethod("SetTrackRating", aimp_manager, rpc_request_handler),
        file_to_save_ratings_(file_to_save_ratings)
    {}

    std::string help()
    {
        return ""
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    std::wstring file_to_save_ratings_;
};

/*! 
    \brief Returns versions of plugin and AIMP player.
    \return object with versions:
            Example: \code {"aimp_version":"3.20.1 Build 1165","plugin_version":"1.0.8.1056"} \endcode
*/
class Version : public AIMPRPCMethod
{
public:
    Version(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("Version", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Returns plugin capabilities.
    \return object which describes capabilities:
         Example: \code "result":{"physical_track_deletion":false,"upload_track":true} \endcode

*/
class PluginCapabilities : public AIMPRPCMethod
{
public:
    PluginCapabilities(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("PluginCapabilities", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Adds URL to specified playlist.
    \param playlist_id - int. \ref special_ids_sec "More"
    \param url - string
    \return object which describes:
        - success:<BR>
            Example: \code "result":{} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::ADD_URL_TO_PLAYLIST_FAILED

            Example: \code {"error":{"code":27,"message":"IAIMPAddonsPlaylistManager::StorageAddEntries() failed. Result -2147024890"}} \endcode
*/
class AddURLToPlaylist : public AIMPRPCMethod
{
public:
    AddURLToPlaylist(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("AddURLToPlaylist", aimp_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! 
    \brief Removes specified track from playlist.

    Please use PluginCapabilities to determine if such functionality is enabled by user.

    \param track_id - int.
    \param playlist_id - int.
    \param physically - bool, optional. Default: false.
    
    Notice: AIMP 3.51 build 1288 shows confirmation dialog. So it useless for remote control.
    
    \return object which describes:
        - success:<BR>
            Example: \code "result":{} \endcode
        - failure: object which describes error: {code, message}<BR>
            Error codes in addition to \link #Rpc::ERROR_CODES Common errors\endlink:
                - ::REMOVE_TRACK_FAILED
                - ::REMOVE_TRACK_PHYSICAL_DELETION_DISABLED

            Example: \code {"error":{"code":29,"message":"Physical deletion is disabled"}} \endcode
*/
class RemoveTrack : public AIMPRPCMethod
{
public:
    RemoveTrack(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler);

    std::string help()
    {
        return "";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
private:

    bool enable_physical_track_deletion_;
};

class EmulationOfWebCtlPlugin : public AIMPRPCMethod
{
public:
    EmulationOfWebCtlPlugin(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("EmulationOfWebCtlPlugin", aimp_manager, rpc_request_handler)
    {
        initMethodNamesMap();
    }

    std::string help()
    {
        return "see documentation of aimp-web-ctl plugin";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    enum METHOD_ID {
        get_playlist_list,
        get_version_string,
        get_version_number,
        get_update_time,
        get_playlist_songs,
        get_playlist_crc,
        get_player_status,
        get_song_current,
        get_volume,
        set_volume,
        get_track_position,
        set_track_position,
        get_track_length,
        get_custom_status,
        set_custom_status,
        set_song_play,
        set_song_position,
        set_player_status,
        player_play,
        player_pause,
        player_stop,
        player_prevous,
        player_next,
        playlist_sort,
        playlist_add_file,
        playlist_del_file,
        playlist_queue_add,
        playlist_queue_remove,
        download_song
    };

    typedef std::map<std::string, METHOD_ID> MethodNamesMap;

    void initMethodNamesMap();

    const METHOD_ID* getMethodID(const std::string& method_name) const;

    void getPlaylistList(std::ostringstream& out);
    void getPlaylistSongs(int playlist_id, bool ignore_cache, bool return_crc, int offset, int size, std::ostringstream& out);
    void getPlayerStatus(std::ostringstream& out);
    void getCurrentSong(std::ostringstream& out);
    void calcPlaylistCRC(int playlist_id);
    void setPlayerStatus(const std::string& statusType, int value);
    void sortPlaylist(int playlist_id, const std::string& sortType);
    void addFile(int playlist_id, const std::string& filename_url);


    MethodNamesMap method_names_;
};

} // namespace AimpRpcMethods
