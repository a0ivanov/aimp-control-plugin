// Copyright (c) 2011, Alexey Ivanov

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

//! contains RPC methods definitions.
namespace AimpRpcMethods
{

// Use codes in range [11-1000] for RPC methods errors. Rpc server uses range [1-10] for its errors.
enum ERROR_CODES { WRONG_ARGUMENT = 11,
                   PLAYBACK_FAILED, SHUFFLE_MODE_SET_FAILED, REPEAT_MODE_SET_FAILED,
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

/*!
    \brief Base class for all AIMP RPC methods.
           Contains references to AIMPManager and MultiUserModeManager.
*/
class AIMPRPCMethod : public Rpc::Method
{
public:

    AIMPRPCMethod(  const std::string& name,
                    AIMPManager& aimp_manager,
                    MultiUserModeManager& multi_user_mode_manager,
                    Rpc::RequestHandler& rpc_request_handler
                 )
        :
        Rpc::Method(name),
        aimp_manager_(aimp_manager),
        multi_user_mode_manager_(multi_user_mode_manager),
        rpc_request_handler_(rpc_request_handler)
    {}

    virtual ~AIMPRPCMethod() {}

protected:

    AIMPManager& aimp_manager_;
    MultiUserModeManager& multi_user_mode_manager_;
    Rpc::RequestHandler& rpc_request_handler_;

private:

    AIMPRPCMethod();
};


//! Starts playback.
class Play : public AIMPRPCMethod
{
public:
    Play(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("play", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "There are two ways to use 'play' function: "
               "play() plays current track in current playlist. "
               "play_track_in_playlist(int track_id, int playlist_id) plays specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Pauses playback.
class Pause : public AIMPRPCMethod
{
public:
    Pause(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("pause", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "pause() pauses playback.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Stops playback.
class Stop : public AIMPRPCMethod
{
public:
    Stop(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("stop", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "stop() stops playback.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Plays previous track.
class PlayPrevious : public AIMPRPCMethod
{
public:
    PlayPrevious(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("play_previous", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "play_previous() plays previous track.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Plays next track.
class PlayNext : public AIMPRPCMethod
{
public:
    PlayNext(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("play_next", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "play_next() plays next track.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Gets/sets AIMP status.
class Status : public AIMPRPCMethod
{
public:
    Status(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("status", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int status(int statusID, int status_value) sets specified status of player to specified value. "
               "If status_value is not specified returns current requested status.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    bool statusGetSetSupported(AIMPManager::STATUS status) const;
};

//! Gets/sets shuffle playback mode.
class ShufflePlaybackMode : public AIMPRPCMethod
{
public:
    ShufflePlaybackMode(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("shuffle_playback_mode", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool shuffle_playback_mode(bool shuffle_playback_on) activates or deactivates shuffle playback mode. "
               "If no arguments are passed returns current state of shuffle mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Gets/sets repeat playback mode.
class RepeatPlaybackMode : public AIMPRPCMethod
{
public:
    RepeatPlaybackMode(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("repeat_playback_mode", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool repeat_playback_mode(bool repeat_playback_on) activates or deactivates repeat playlist playback mode. "
               "If no arguments are passed returns current state of repeat mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Gets/sets volume level in percents.
class VolumeLevel : public AIMPRPCMethod
{
public:
    VolumeLevel(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("volume", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int volume(int level) sets volume level(in percents). By default returns current volume level.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Gets/sets mute mode.
class Mute : public AIMPRPCMethod
{
public:
    Mute(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("mute", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "bool mute(bool mute_on) activates or deactivates mute mode. "
               "If no arguments are passed returns current state of repeat mode.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*!
    \return formatted string for specified entry. See AIMPManager::getFormattedEntryTitle() for details about format string.
*/
class GetFormattedEntryTitle : public AIMPRPCMethod
{
public:
    GetFormattedEntryTitle(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("get_formatted_entry_title", aimp_manager, multi_user_mode_manager, rpc_request_handler)
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
                "Param callbacks - see description at AimpManager commentaries."
            "Result - is object with member 'formatted_string', string."
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! \return all info about entry.
class GetPlaylistEntryInfo : public AIMPRPCMethod
{
public:
    GetPlaylistEntryInfo(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("get_playlist_entry_info", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "get_playlist_entry_info(int playlist_id, int track_id) returns info about track represented as struct with following fields:"
               " 'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'."
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Enqueues track with track_id in playlist with playlist_id for playing.
class EnqueueTrack : public AIMPRPCMethod
{
public:
    EnqueueTrack(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("enqueue_track", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "enqueue_track(int track_id, int playlist_id, bool insert_at_queue_beginning = false) "
               "enqueues for playing specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

//! Removes track with track_id in playlist with playlist_id from AIMP play queue.
class RemoveTrackFromPlayQueue : public AIMPRPCMethod
{
public:
    RemoveTrackFromPlayQueue(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("remove_track_from_play_queue", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "remove_track_from_play_queue(int track_id, int playlist_id) "
               "removes from AIMP play queue specified track in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*! TODO: add description here
    \return list of playlists. Set of fields for each playlist should be specified.
    Available arguments are: 'id', 'title', 'duration', 'entries_count', 'size_of_entries'.
*/
class GetPlaylists : public AIMPRPCMethod
{
public:

    GetPlaylists(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        :
        AIMPRPCMethod("get_playlists", aimp_manager, multi_user_mode_manager, rpc_request_handler),
        playlist_fields_filler_("playlist")
    {
        using namespace RpcValueSetHelpers;
        using namespace RpcResultUtils;
        boost::assign::insert(playlist_fields_filler_.setters_)
            ( getStringFieldID(Playlist::ID),                           boost::bind( createSetter(&Playlist::getID),                      _1, _2 ) )
            ( getStringFieldID(Playlist::TITLE),                        boost::bind( createSetter(&Playlist::getTitle),                   _1, _2 ) )
            ( getStringFieldID(Playlist::DURATION),                     boost::bind( createSetter(&Playlist::getDuration),                _1, _2 ) )
            ( getStringFieldID(Playlist::ENTRIES_COUNT),                boost::bind( createSetter(&Playlist::getEntriesCount),            _1, _2 ) )
            ( getStringFieldID(Playlist::SIZE_OF_ALL_ENTRIES_IN_BYTES), boost::bind( createSetter(&Playlist::getSizeOfAllEntriesInBytes), _1, _2 ) )
        ;
    }

    std::string help()
    {
        return "get_playlists(string playlist_fields[]) returns array of playlists. "
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

/*! TODO: add description here
    \return list of entries in specified playlist. Set of fields for each entry should be specified after playlist id parameter.
    Available arguments are: "id", "title", "artist", "album", "date", "genre", "bitrate", "duration", "filesize", "rating".
    Field 'id' is key of entry in AIMPManager not real internal AIMP track ID.
*/
class GetPlaylistEntries : public AIMPRPCMethod
{
public:
    GetPlaylistEntries(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        :
        AIMPRPCMethod("get_playlist_entries", aimp_manager, multi_user_mode_manager, rpc_request_handler),
        kTOTAL_ENTRIES_COUNT_RPCVALUE_KEY("total_entries_count"),
        kENTRIES_RPCVALUE_KEY("entries"),
        entry_fields_filler_("entry"),
        kFIELD_INDEX_RPCVALUE("field_index"),
        kDESCENDING_ORDER_RPCVALUE("desc"),
        kORDER_DIRECTION_RPCVALUE("dir"),
        kCOUNT_OF_FOUND_ENTRIES_RPCVALUE_KEY("count_of_found_entries")
    {
        using namespace RpcValueSetHelpers;
        using namespace RpcResultUtils;
        boost::assign::insert(entry_fields_filler_.setters_)
            ( getStringFieldID(PlaylistEntry::ID),       boost::bind( createSetter(&PlaylistEntry::getID),       _1, _2 ) )  // Use plugin id of entry instead Aimp internal id( PlaylistEntry::getTrackID() ).
            ( getStringFieldID(PlaylistEntry::TITLE),    boost::bind( createSetter(&PlaylistEntry::getTitle),    _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::ARTIST),   boost::bind( createSetter(&PlaylistEntry::getArtist),   _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::ALBUM),    boost::bind( createSetter(&PlaylistEntry::getAlbum),    _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::DATE),     boost::bind( createSetter(&PlaylistEntry::getDate),     _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::GENRE),    boost::bind( createSetter(&PlaylistEntry::getGenre),    _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::BITRATE),  boost::bind( createSetter(&PlaylistEntry::getBitrate),  _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::DURATION), boost::bind( createSetter(&PlaylistEntry::getDuration), _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::FILESIZE), boost::bind( createSetter(&PlaylistEntry::getFileSize), _1, _2 ) )
            ( getStringFieldID(PlaylistEntry::RATING),   boost::bind( createSetter(&PlaylistEntry::getRating),   _1, _2 ) )
        ;

        // initialization of fields available to order.
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
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::getTitle,  _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::getArtist, _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::getAlbum,  _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::getDate,   _1) );
        fields_to_filter_getters.push_back( boost::bind(&PlaylistEntry::getGenre,  _1) );
    }

    std::string help()
    {
        return "get_playlist_entries(int playlist_id, string entry_fields[], int start_index, int entries_count, struct entry_fields_to_order[], string search_string) "
               "returns struct with following members: "
               "    'count_of_found_entries' - int value, used if search_string is not empty; "
               "    'entries' - array of entries where each entry is associative array with following keys: "
               "            'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'. "
               "Get sub range of entries: start_index and entries_count used to get part of entries instead whole playlist. "
               "Ordering: entry_fields_to_order is array of field descriptions, used to order entries by multiple fields. Each descriptor is associative array with keys: "
               "    'field_index' - index of field to order in entry_fields[] array; "
               "    'dir' - order('asc' - ascending, 'desc' - descending). "
               "Filtering: only those entries will be returned which have at least one occurence of search_string in one of entry string field (title, artist, album, data, genre).";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);

private:

    /* \return start entry index. */
    const size_t getStartFromIndexFromRpcParam(int start_from_index, size_t max_value); // throws Rpc::Exception

    /* \return count of entries to process. */
    const size_t getEntriesCountFromRpcParam(int entries_count, size_t max_value); // throws Rpc::Exception

    // See HelperFillRpcFields class commentaries.
    RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry> entry_fields_filler_;

    // rpc result struct keys.
    const std::string kENTRIES_RPCVALUE_KEY; // key for entries array.
    const std::string kTOTAL_ENTRIES_COUNT_RPCVALUE_KEY; // key for total entries count.

    // entries ordering
    const std::string kFIELD_INDEX_RPCVALUE;
    const std::string kDESCENDING_ORDER_RPCVALUE;
    const std::string kORDER_DIRECTION_RPCVALUE;
    typedef std::map<std::string, ENTRY_FIELDS_ORDERABLE> FieldsToOrderMap;
    FieldsToOrderMap fields_to_order_;
    EntriesSortUtil::FieldToOrderDescriptors field_to_order_descriptors_;
    void fillFieldToOrderDescriptors(const Rpc::Value& entry_fields_to_order);

    // entries filtering
    const std::string kCOUNT_OF_FOUND_ENTRIES_RPCVALUE_KEY;
    PlaylistEntryIDList filtered_entries_ids_;
    PlaylistEntryIDList default_order_entries_ids_;
    std::wstring search_string_;

    /*
        Get search string from Rpc param.
        \return true if search string is not empty.
    */
    bool getSearchStringFromRpcParam(const std::string& search_string_utf8);
    FindStringOccurenceInEntryFieldsFunctor entry_contain_string_;
    const PlaylistEntryIDList& getEntriesIDsFilteredByStringFromEntriesList(const std::wstring& search_string,
                                                                            const EntriesListType& entries);
    const PlaylistEntryIDList& getEntriesIDsFilteredByStringFromEntryIDs(const std::wstring& search_string,
                                                                         const PlaylistEntryIDList& entry_to_filter_ids,
                                                                         const EntriesListType& entries);
    void outputFilteredEntries(const PlaylistEntryIDList& filtered_entries_ids, const EntriesListType& entries, size_t start_entry_index, size_t entries_count, Rpc::Value& result, Rpc::Value& rpcvalue_entries);

    // entries output

    /* Fills rpcvalue array of entries from entries objects. */
    void fillRpcValueEntriesFromEntriesList(const EntriesListType& entries, size_t start_entry_index, size_t entries_count, Rpc::Value& rpcvalue_entries);

    /* Fills rpcvalue array of entries from entries IDs. */
    void fillRpcValueEntriesFromEntryIDs(const PlaylistEntryIDList& entries_ids, const EntriesListType& entries, size_t start_entry_index, size_t entries_count, Rpc::Value& rpcvalue_entries);
};

/*!
    \return count of entries in specified playlist.
    \param [0] int playlist ID.
*/
class GetPlaylistEntriesCount : public AIMPRPCMethod
{
public:
    GetPlaylistEntriesCount(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("get_playlist_entries_count", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "int get_playlist_entries_count(int playlist_id) returns count of entries in specified playlist.";
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*!
    \return cover of specified track in specified playlist.
    \param [0] int track ID.
    \param [1] int playlist ID.
    \param [2] int cover width, optional.
    \param [3] int cover height, optional.
*/
class GetCover : public AIMPRPCMethod
{
public:
    GetCover(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler, const std::wstring& document_root, const std::wstring& cover_directory)
        :
        AIMPRPCMethod("get_cover", aimp_manager, multi_user_mode_manager, rpc_request_handler),
        document_root_(document_root),
        cover_directory_(cover_directory),
        die_( rng_engine_, random_range_ ) // init generator by range[0, 9]
    {
        random_file_part_.resize(kRANDOM_FILENAME_PART_LENGTH);
    }

    std::string help()
    {
        return "get_cover(int track_id, int playlist_id, int width, int height) "
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

/*! \return current state of AIMP control panel. */
class GetPlayerControlPanelState : public AIMPRPCMethod
{
public:
    GetPlayerControlPanelState(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("get_control_panel_state", aimp_manager, multi_user_mode_manager, rpc_request_handler)
    {}

    std::string help()
    {
        return "get_control_panel_state() returns current AIMP control panel state."
               "\return structure with following members:"
               "1) "
               // TODO: add description here
        ;
    }

    Rpc::ResponseType execute(const Rpc::Value& root_request, Rpc::Value& root_response);
};

/*!
    \brief Comet technique implementation.
    This function returns result as soon as(not immediatelly) specified event occures.
    \param  string event ID.
            Supported events are:
                1) 'play_state_change' - playback state change event (player switch to playing/paused/stopped state)
                Response will contain following members:
                    'playback_state', string - playback state (playing, stopped, paused)
                2) 'current_track_change' - current track change event (player switched track)
                Response will contain following members:
                    1) 'playlist_id', int - playlist ID
                    2) 'track_id', int - track ID
                3) 'control_panel_state_change' - one of following events:
                        playback state, mute, shuffle, repeat, volume level change
                Response will be the same as get_control_panel_state() function.
*/
class SubscribeOnAIMPStateUpdateEvent : public AIMPRPCMethod
{
public:
    SubscribeOnAIMPStateUpdateEvent(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("subscribe", aimp_manager, multi_user_mode_manager, rpc_request_handler)
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
        return "subscribe(string event_id) returns AIMP state descriptor when event (specified by id) occures."
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

//! Set track rating.
/*!
    Current implementation just save path to track and rating in text file since AIMP SDK does not suport rating set now.
    No checks performed(ex. rating already exists for file and etc.) since it is just temporarily stub.
*/
class SetTrackRating : public AIMPRPCMethod
{
public:
    SetTrackRating(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler, const std::wstring& file_to_save_ratings)
        : AIMPRPCMethod("set_track_rating", aimp_manager, multi_user_mode_manager, rpc_request_handler),
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
    EmulationOfWebCtlPlugin(AIMPManager& aimp_manager, MultiUserModeManager& multi_user_mode_manager, Rpc::RequestHandler& rpc_request_handler)
        : AIMPRPCMethod("emulation_of_webctl_plugin", aimp_manager, multi_user_mode_manager, rpc_request_handler)
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
        playlist_del_file
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
