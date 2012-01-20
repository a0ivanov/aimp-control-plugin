// Copyright (c) 2011, Alexey Ivanov
/*! 
    \file
    \mainpage AIMP Control RPC functionality
*/
#ifndef RPC_METHODS_H
#define RPC_METHODS_H

#include "aimp/manager.h"
#include "method.h"
#include "value.h"
#include "utils.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std.hpp>
#include <boost/bind.hpp>

namespace MultiUserMode { class MultiUserModeManager; }

namespace Rpc { class DelayedResponseSender; }

// contains RPC methods definitions.
namespace AimpRpcMethods
{

/*!
    \page pg_common_errors Common errors
    
    Rpc::Value's and request parsing errors:
        - REQUEST_PARSING_ERROR  = 1
        - METHOD_NOT_FOUND_ERROR = 2
        - TYPE_ERROR             = 3
        - INDEX_RANGE_ERROR      = 4
        - OBJECT_ACCESS_ERROR    = 5
        - VALUE_RANGE_ERROR      = 6
*/

/*! 
    \brief Error codes which RPC functions return.    
    \internal Use codes in range [11-1000] for RPC methods errors. Rpc server uses range [1-10] for its errors. \endinternal
*/
enum ERROR_CODES { WRONG_ARGUMENT = 11, /*!< returned when arguments passed in function can't be processed. Reasons: missing required arg, wrong type. */
                   PLAYBACK_FAILED, /*!< returned when AIMP can't start playback of requested track. */
                   SHUFFLE_MODE_SET_FAILED, REPEAT_MODE_SET_FAILED,
                   VOLUME_OUT_OF_RANGE, VOLUME_SET_FAILED, MUTE_SET_FAILED,
                   ENQUEUE_TRACK_FAILED, DEQUEUE_TRACK_FAILED,
                   PLAYLIST_NOT_FOUND,
                   TRACK_NOT_FOUND,
                   ALBUM_COVER_LOAD_FAILED,
                   RATING_SET_FAILED,
                   STATUS_SET_FAILED
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

    AIMPManager& aimp_manager_;
    Rpc::RequestHandler& rpc_request_handler_;

private:

    AIMPRPCMethod();
};

/*! 
    \brief Starts playback.
    \param track_id - int
    \param playlist_id - int
    \remark If no params is specified, starts playback of current track in current playlist.
    \return object which describes:
                - success: current track.
                  Example:
\code
{ "track_id" : 0, "playlist_id" : 2136855104 }
\endcode
                - failure: object which describes error. Can return error ::PLAYBACK_FAILED in addtion to \ref pg_common_errors.
                  Example:
\code
{"error":{"code":12,
          "message":"Playback specified track failed. Track does not exist or track's source is not available."}
}
\endcode
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
      - VOLUME      = 1,  [0,100]
      - BALANCE     = 2,
      - SPEED       = 3,
      - Player      = 4,
      - MUTE        = 5,  [0, 1]
      - REVERB      = 6,
      - ECHO        = 7,
      - CHORUS      = 8,
      - Flanger     = 9,
      - EQ_STS      = 10,
      - EQ_SLDR01   = 11,
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
      - REPEAT      = 29,  [0, 1]
      - ON_STOP     = 30,
      - POS         = 31,  [0, LENGTH)
      - LENGTH      = 32,
      - REPEATPLS   = 33,  [0, 1]
      - REP_PLS_1   = 34,  [0, 1]
      - KBPS        = 35,
      - KHZ         = 36,
      - MODE        = 37,
      - RADIO       = 38,
      - STREAM_TYPE = 39,
      - TIMER       = 40,
      - SHUFFLE     = 41   [0, 1]
    \param value - int, optional: new status value. Value range depends on status ID. If value is not specified, function returns current status.
    \return object which describes current status value.
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
    \brief Returns formatted string for specified track.

           Acts like printf() analog, see detailes below.
    \param track_id - int.
    \param playlist_id - int.
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
    \param track_id - int
    \param playlist_id - int.
    \return object which describes specified track.
            Example:\code{"album":"Word Gets Around","artist":"Stereophonics","bitrate":320,"date":"1997","duration":182961,"filesize":7318732,"genre":"","id":0,"rating":5,"title":"A Thousand Trees"}}\endcode
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
    \param track_id - int
    \param playlist_id - int
    \param insert_at_queue_beginning - boolean, optional, default is false. If true enqueue at the beginning of queue.
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
    \param track_id - int
    \param playlist_id - int
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
    \brief Returns list of playlists.
    \param fields - array of strings, optional. List of fields that need to be filled.
           Available fields are:
            - id
            - title
            - duration
            - entries_count
            - size_of_entries

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
        boost::assign::insert(playlist_fields_filler_.setters_)
            ( getStringFieldID(Playlist::ID),                           boost::bind( createSetter(&Playlist::id),                      _1, _2 ) )
            ( getStringFieldID(Playlist::TITLE),                        boost::bind( createSetter(&Playlist::title),                   _1, _2 ) )
            ( getStringFieldID(Playlist::DURATION),                     boost::bind( createSetter(&Playlist::duration),                _1, _2 ) )
            ( getStringFieldID(Playlist::ENTRIES_COUNT),                boost::bind( createSetter(&Playlist::entriesCount),            _1, _2 ) )
            ( getStringFieldID(Playlist::SIZE_OF_ALL_ENTRIES_IN_BYTES), boost::bind( createSetter(&Playlist::sizeOfAllEntriesInBytes), _1, _2 ) )
        ;
    }

    std::string help()
    {
        return "GetPlaylists(string playlist_fields[]) returns array of playlists. "
               "Playlist can have following fields: "
                   "'id', 'title', 'duration', 'entries_count', 'size_of_entries'."
               "Required fields passed in playlist_fields string array."
               "Each playlist in response is associative array with keys specified in playlist_fields[].";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    //! See HelperFillRpcFields class commentaries.
    RpcValueSetHelpers::HelperFillRpcFields<Playlist> playlist_fields_filler_;
};

namespace PlaylistEntries {
typedef std::set<std::string> SupportedFieldNames;
typedef std::vector<SupportedFieldNames::const_iterator> RequiredFieldNames;
}

typedef boost::sub_range<const EntriesListType> EntriesRange;
typedef boost::sub_range<const PlaylistEntryIDList> EntriesIDsRange;

typedef boost::function<void(EntriesRange)> EntriesHandler;
typedef boost::function<void(EntriesIDsRange, const EntriesListType&)> EntryIDsHandler;
typedef boost::function<void(size_t)> EntriesCountHandler;


// Fetches list of entries to further processing by GetPlaylistEntries and GetDataTablePageForEntry methods.
class GetPlaylistEntriesTemplateMethod : boost::noncopyable
{
public:

    GetPlaylistEntriesTemplateMethod(AIMPManager& aimp_manager)
        :
        aimp_manager_(aimp_manager),
        kENTRIES_COUNT_STRING("entries_count"),
        kFIELD_STRING("field"),
        kDESCENDING_ORDER_STRING("desc"),
        kORDER_DIRECTION_STRING("dir"),
        kORDER_FIELDS_STRING("order_fields"),
        kSEARCH_STRING_STRING("search_string")
    {
        using namespace RpcResultUtils;

        // initialization of fields available to order.
        fields_to_order_[ getStringFieldID(PlaylistEntry::ID)       ] = AIMPPlayer::ID;
        fields_to_order_[ getStringFieldID(PlaylistEntry::TITLE)    ] = AIMPPlayer::TITLE;
        fields_to_order_[ getStringFieldID(PlaylistEntry::ARTIST)   ] = AIMPPlayer::ARTIST;
        fields_to_order_[ getStringFieldID(PlaylistEntry::ALBUM)    ] = AIMPPlayer::ALBUM;
        fields_to_order_[ getStringFieldID(PlaylistEntry::DATE)     ] = AIMPPlayer::DATE;
        fields_to_order_[ getStringFieldID(PlaylistEntry::GENRE)    ] = AIMPPlayer::GENRE;
        fields_to_order_[ getStringFieldID(PlaylistEntry::BITRATE)  ] = AIMPPlayer::BITRATE;
        fields_to_order_[ getStringFieldID(PlaylistEntry::DURATION) ] = AIMPPlayer::DURATION;
        fields_to_order_[ getStringFieldID(PlaylistEntry::FILESIZE) ] = AIMPPlayer::FILESIZE;
        fields_to_order_[ getStringFieldID(PlaylistEntry::RATING)   ] = AIMPPlayer::RATING;

        field_to_order_descriptors_.reserve( fields_to_order_.size() );

        // initialization of fields available to filtering.
        GettersOfEntryStringField& fields_to_filter_getters = entry_contain_string_.field_getters_;
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::title,  _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::artist, _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::album,  _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::date,   _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::genre,  _1) );
    }

    std::string help()
    {
        return "Get sub range of entries: start_index and entries_count used to get part of entries instead whole playlist. "
               "Ordering: order_fields is array of field descriptions, used to order entries by multiple fields. Each descriptor is associative array with keys: "
               "    'field' - field to order name. Available fields are: id, title, artist, album, date, genre, bitrate, duration, filename, rating; "
               "    'dir'   - order('asc' - ascending, 'desc' - descending); "
               "Filtering: only those entries will be returned which have at least one occurence of search_string in one of entry string field (title, artist, album, data, genre).";
    }

    Rpc::ResponseType execute(const Rpc::Value& params,
                              // following needed by only by GetPlaylistEntries
                              EntriesHandler entries_handler,
                              EntryIDsHandler entry_ids_handler,
                              EntriesCountHandler total_entries_count_handler,
                              EntriesCountHandler filtered_entries_count_handler,
                              // following needed by only by GetEntryPositionInDataTable
                              EntriesHandler full_entries_list_handler,
                              EntryIDsHandler full_entry_ids_list_handler,
                              EntriesCountHandler page_size_handler
                              );

    void setSupportedFieldNames(const PlaylistEntries::SupportedFieldNames& supported_fields_names)
        { supported_fields_names_ = supported_fields_names; }

private:
    
    AIMPManager& aimp_manager_;

    PlaylistEntries::SupportedFieldNames supported_fields_names_; // list of entry field names which can be handled.
    
    //! \return start entry index.
    const size_t getStartFromIndexFromRpcParam(int start_from_index, size_t max_value); // throws Rpc::Exception

    //! \return count of entries to process.
    const size_t getEntriesCountFromRpcParam(int entries_count, size_t max_value); // throws Rpc::Exception
    const std::string kENTRIES_COUNT_STRING;

    // entries ordering
    const std::string kFIELD_STRING,
                      kDESCENDING_ORDER_STRING,
                      kORDER_DIRECTION_STRING,
                      kORDER_FIELDS_STRING;
    typedef std::map<std::string, ENTRY_FIELDS_ORDERABLE> FieldsToOrderMap;
    FieldsToOrderMap fields_to_order_;
    EntriesSortUtil::FieldToOrderDescriptors field_to_order_descriptors_;
    void fillFieldToOrderDescriptors(const Rpc::Value& entry_fields_to_order);

    // entries filtering
    const std::string kSEARCH_STRING_STRING;
    PlaylistEntryIDList filtered_entries_ids_;
    std::wstring search_string_;

    //!\return true if search string is not empty.
    bool getSearchStringFromRpcParam(const std::string& search_string_utf8);
    FindStringOccurenceInEntryFieldsFunctor entry_contain_string_;
    const PlaylistEntryIDList& getEntriesIDsFilteredByStringFromEntriesList(const std::wstring& search_string,
                                                                            const EntriesListType& entries);
    const PlaylistEntryIDList& getEntriesIDsFilteredByStringFromEntryIDs(const std::wstring& search_string,
                                                                         const PlaylistEntryIDList& entry_to_filter_ids,
                                                                         const EntriesListType& entries);

    // entries handling
    void handleFilteredEntryIDs(const PlaylistEntryIDList& filtered_entries_ids, const EntriesListType& entries,
                                size_t start_entry_index, size_t entries_count,
                                EntryIDsHandler entry_ids_handler,
                                EntryIDsHandler full_entry_ids_list_handler,
                                EntriesCountHandler filtered_entries_count_handler);

    void handleEntries(const EntriesListType& entries, size_t start_entry_index, size_t entries_count,
                       EntriesHandler entries_handler,
                       EntriesHandler full_entries_list_handler);

    void handleEntryIDs(const PlaylistEntryIDList& entries_ids, const EntriesListType& entries,
                        size_t start_entry_index, size_t entries_count,
                        EntryIDsHandler entry_ids_handler,
                        EntryIDsHandler full_entry_ids_list_handler);
};

/*! 
    \brief Returns list of playlist entries.
    \param playlist_id - int
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

    \return object which describes playlist entry.
            Example:\code{"count_of_found_entries":1,"entries":[[1,"Looks Like Chaplin"]],"total_entries_count":3}}\endcode
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
               + get_playlist_entries_templatemethod_->help();
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

    // we must share this object with GetEntryPositionInDataTable method.
    boost::shared_ptr<GetPlaylistEntriesTemplateMethod> getPlaylistEntriesTemplateMethod()
        { return get_playlist_entries_templatemethod_; };

private:

    boost::shared_ptr<GetPlaylistEntriesTemplateMethod> get_playlist_entries_templatemethod_;

    // See HelperFillRpcFields class commentaries.
    RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry> entry_fields_filler_;
    void initEntriesFiller(const Rpc::Value& params);

    const std::string kFORMAT_STRING_STRING;

    const std::string kFIELDS_STRING;

    // rpc result struct keys.
    const std::string kENTRIES_STRING; // key for entries array.
    const std::string kTOTAL_ENTRIES_COUNT_STRING; // key for total entries count.
    const std::string kCOUNT_OF_FOUND_ENTRIES_STRING;

    //! Fills rpcvalue array of entries from entries objects.
    void fillRpcValueEntriesFromEntriesList(EntriesRange entries_range,
                                            Rpc::Value& rpcvalue_entries);

    //! Fills rpcvalue array of entries from entries IDs.
    void fillRpcValueEntriesFromEntryIDs(EntriesIDsRange entries_ids_range, const EntriesListType& entries,
                                         Rpc::Value& rpcvalue_entries);
    
    EntriesHandler entries_handler_stub_;
    EntryIDsHandler enties_ids_handler_stub_;
    EntriesCountHandler entries_count_handler_stub_;
};

/*! 
    \brief Returns page number and index of track on page.
    \param track_id - int
    \param playlist_id - int
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

    boost::shared_ptr<GetPlaylistEntriesTemplateMethod> get_playlist_entries_templatemethod_;

    // following members are valid only while execute() method is running after calling getplaylistentries_method_.execute().
    size_t entries_on_page_;
    int entry_index_in_current_representation_; // index of entry in reperesentation(concrete filtering and sorting) of playlist entries.

    void setEntryPageInDataTableFromEntriesList(EntriesRange entries_range,
                                                PlaylistEntryID entry_id
                                                );

    void setEntryPageInDataTableFromEntryIDs(EntriesIDsRange entries_ids_range, const EntriesListType& /*entries*/,
                                             PlaylistEntryID entry_id);

    EntriesHandler entries_handler_stub_;
    EntryIDsHandler enties_ids_handler_stub_;
    EntriesCountHandler entries_count_handler_stub_;
    EntriesCountHandler entries_on_page_count_handler_;
};

/*! 
    \brief Returns count of entries in playlist.
    \param playlist_id - int
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
    \brief Returns URI of album cover.
    \param playlist_id - int
    \param track_id - int
    \param cover_width - int, optional.
    \param cover_height - int, optional.
    
    \return URI of cover.
            Example:\code{"album_cover_uri":"tmp/cover_35_2136855104_0x0_99263.png"}\endcode 
    \remark Function is available only if FreeImage.dll and FreeImagePlus.dll are available for loading by AIMP.
*/
class GetCover : public AIMPRPCMethod
{
public:
    GetCover(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler, const std::wstring& document_root, const std::wstring& cover_directory)
        :
        AIMPRPCMethod("GetCover", aimp_manager, rpc_request_handler),
        document_root_(document_root),
        cover_directory_(cover_directory),
        die_( rng_engine_, random_range_ ) // init generator by range[0, 9]
    {
        random_file_part_.resize(kRANDOM_FILENAME_PART_LENGTH);
    }

    std::string help()
    {
        return "GetCover(int track_id, int playlist_id, int cover_width, int cover_height) "
               "returns URI of PNG cover(size is determined by width and height arguments, pass zeros to get full size cover) "
               "of specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    std::wstring document_root_;
    std::wstring cover_directory_;

    /*
        Generate filename in format cover_playlistID_trackID_widthxheight_random. Ex: cover_2222222_01_100x100_45730.
    */
    std::wstring getTempFileNameForAlbumCover(TrackDescription track_desc, std::size_t width, std::size_t height);

    //  random utils
    typedef boost::variate_generator<boost::mt19937&, boost::uniform_int<> > RandomNumbersGenerator;
    static const int kRANDOM_FILENAME_PART_LENGTH = 5;
    boost::uniform_int<> random_range_;
    boost::mt19937 rng_engine_;
    RandomNumbersGenerator die_;
    std::wstring random_file_part_;

    // cover files map
    typedef std::list<std::wstring> FilenamesList;
    typedef std::map<TrackDescription, FilenamesList> CoverFilenames;
    CoverFilenames cover_filenames_;
    const std::wstring* isCoverExistsInCoverDirectory(TrackDescription track_desc, std::size_t width, std::size_t height) const;
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
                        playback state, mute, shuffle, repeat, volume level.
                - 'playlists_content_change' - change of: playlist's content, playlist count.
                Response will be the same as GetPlayerControlPanelState function.
                Response example:\code{"mute_mode_on":false,"playback_state":"playing","playlist_id":2136855104,"repeat_mode_on":false,"shuffle_mode_on":true,"track_id":13,"track_length":174,"track_position":0,"volume":49}\endcode
*/
class SubscribeOnAIMPStateUpdateEvent : public AIMPRPCMethod
{
public:
    SubscribeOnAIMPStateUpdateEvent(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("SubscribeOnAIMPStateUpdateEvent", aimp_manager, rpc_request_handler)
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
                           "playback state, mute, shuffle, repeat, volume level change"
                       "Response will be the same as get_control_panel_state() function."
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
};

/*!
    \brief Set track rating.
    On AIMP3: full functional.
    On AIMP2: 
        Current implementation just saves path to track and rating in text file since AIMP SDK does not support rating change now.
        No checks are performed(ex. rating already exists for file and etc.) since it is temporarily stub.
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

#endif // #ifndef RPC_METHODS_H
