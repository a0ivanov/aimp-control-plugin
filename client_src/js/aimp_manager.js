/*
    Script contains AimpManager class definition used for AIMP Control plugin frontend to communicate with AIMP RPC(Xmlrpc/Jsonrpc) server.
    Copyright (c) 2011, Alexey Ivanov
*/

/*
    Information about AimpManager's methods call:
        All methods have last argument 'callbacks' - object with following members:
            1) on_success - function(result)
                function is called on successfull RPC method call.
                Arguments:
                    result - object with members dependent on specific function.
            2) on_exception - function(error, localized_error_message)
                function is called on failure RPC method call.
                Arguments:
                    error - object with members (int code, string message)
                    localized_error_message - localized string error description.
                Function should return true if exception was handled, and false otherwise.
            3) on_complete - function(response)
                function is called unconditionally after on_success() or on_exception().
                Arguments:
                    response - object with members:
                        id - do not know
                        result - the same object that was passed to on_success() or on_exception()
*/

// AimpManager constructor
function AimpManager() {
    this.aimp_service = new rpc.ServiceProxy(
                               '/RPC_JSON', // RPC URI.
                               {
                                /*options*/
                                protocol : 'JSON-RPC',
                                //sanitize : false,
                                methods: ['play', 'pause', 'stop', 'play_previous', 'play_next', // control panel utils
                                          'shuffle_playback_mode', 'repeat_playback_mode',
                                          'get_control_panel_state',
                                          'volume', 'mute', // volume utils
                                          'status', // get/set various aspects of player
                                          //'enqueue_track', 'remove_track_from_play_queue', // queue tracks
                                          'get_playlists', 'get_playlist_entries', 'get_playlist_entries_count', 'get_formatted_entry_title', 'get_playlist_entry_info', 'set_track_rating', // playlists and tracks utils
                                          'get_cover', // album cover getter
                                          'subscribe' // subscribe for AIMP player state notifications
                                         ]
                               }
    );

    function initLocalizedErrorMessages() {
        // syncronize RPC error codes to localized string IDs.
        // Codes in range [1-1000] for RPC methods errors.
        /*
        enum ERROR_CODES { REQUEST_PARSING_ERROR = 1, METHOD_NOT_FOUND_ERROR,
                           TYPE_ERROR, INDEX_RANGE_ERROR, OBJECT_ACCESS_ERROR, VALUE_RANGE_ERROR
        };
        enum ERROR_CODES { WRONG_ARGUMENT = 11,
                           PLAYBACK_FAILED, SHUFFLE_MODE_SET_FAILED, REPEAT_MODE_SET_FAILED,
                           VOLUME_OUT_OF_RANGE, VOLUME_SET_FAILED, MUTE_SET_FAILED,
                           ENQUEUE_TRACK_FAILED, DEQUEUE_TRACK_FAILED,
                           PLAYLIST_NOT_FOUND,
                           ALBUM_COVER_LOAD_FAILED,
                           RATING_SET_FAILED,
                           STATUS_SET_FAILED
          };
        */

        this.error_codes_to_messages_map = {
            1 : 'error_request_parsing',
            2 : 'error_method_not_found',
            3 : 'error_type',
            4 : 'error_index_range',
            5 : 'error_object_access',
            6 : 'error_value_range',
            11 : 'error_wrong_argument',
            12 : 'error_playback_failed',
            13 : 'error_shuffle_mode_set_failed',
            14 : 'error_repeat_mode_set_failed',
            15 : 'error_volume_out_of_range',
            16 : 'error_volume_set_failed',
            17 : 'error_mute_set_failed',
            18 : 'error_enqueue_track_failed',
            19 : 'error_dequeue_track_failed',
            20 : 'error_playlist_not_found',
            21 : 'error_track_not_found',
            22 : 'error_album_cover_load_failed',
            23 : 'error_rating_set_failed',
            24 : 'error_status_set_failed'
        };
    }
    initLocalizedErrorMessages();
}

AimpManager.prototype = {

/*
How to call RPC method of "JavaScript RPC Client":

this.aimp_service.method_name({
    // params can be array or object(JSON-RPC only).
    params : [], // {}
    // function is called on successfull RPC method call.
    onSuccess : function(result) {}, // result - object with members dependent on specific function.
    // function is called on failure RPC method call.
    onException : function(error) {}, // error - object with members (int code, string message)
    // function is called after onSuccess() or onException().
    onComplete : function(response) {
        // response - object with members:
        //    id - do not know
        //    result - the same object that was passed to onSuccess() or onException()

        // any 'final' logic
    }
});
*/


/*
    This function is called on RPC method call exception.
    Invokes client 'on exception' handler with additional parameter - localized RPC error message string.
*/
onRpcException : function (on_exception_client) {
    return function (error) {
        // return true if exception was handled, false otherwise.
        if (on_exception_client === undefined) {
            return false;
        }

        // map int error code to localized message ID.
        var localized_error_message =
            error_codes_to_messages_map.hasOwnProperty(error.code) ? getText(error_codes_to_messages_map[error.code])
                                                                   : getText('error_unknown') + ' ' + error.code;
        // call client callback if defined.
        return on_exception_client(error, localized_error_message);
    };
},

callRpc : function(method, params, callbacks) {
    method({ params : params,
             onSuccess : callbacks.on_success,
             onException : this.onRpcException(callbacks.on_exception),
             onComplete : callbacks.on_complete
           }
    );
},

/*
    Returns array of playlists.
        Param params.fields - array of strings represents field IDs.
            Available field IDs: 'id', 'title', 'duration', 'entries_count', 'size_of_entries'.
        Param callbacks - see description in AimpManager comments.
    Result is array of objects with members specified by params.fields param.
*/
getPlaylists : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_playlists, params, callbacks);
},

/*
    Returns array of playlist entries.
        Param params.playlist_id - playlist ID.
        Param params.fields - array of fields each playlist entry will contain.
            Available fields are: 'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'.
        Param params.start_index - index of first entry. Default: 0.
        Param params.entries_count - count of entries. Default: all entries.
        Param params.order_fields - array of field descriptions, used to order entries by multiple fields.
                Each descriptor is object with members:
                    'field_index' - index of field to order in params.fields array.
                    'dir' - order('asc' - ascending, 'desc' - descending)
        Param params.search_string - only those entries will be returned which have at least
                     one occurence of params.search_string in one of entry string field (title, artist, album, data, genre).

        Param callbacks - see description in AimpManager comments.
    Result is object with following members:
        total_entries_count - total entries count in playlist
        entries - array of entries. Entry is object with members specified by params.fields.
        count_of_found_entries(optional value. Defined if params.search_string is not empty) - count of entries
            which match params.search_string. See params.search_string param description for details.
*/
getPlaylistEntries : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_playlist_entries, params, callbacks);
},

/*
    Returns playlist entries count.
        Param params.playlist_id - playlist ID.
        Param callbacks - see description in AimpManager comments.
*/
getPlaylistEntriesCount : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_playlist_entries_count, params, callbacks);
},

/*
    Returns formatted string for specified track in specified playlist.
        Param params.track_id - track ID.
        Param params.playlist_id - playlist ID.
        Param params.format_string - format string. There are following format arguments:
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
            %IF(A, B, C) - if A is empty use C else use B.
        Example: format_string = "%a - %T", result = "artist - title".
        Param callbacks - see description in AimpManager comments.
    Result - is object with member 'formatted_string', string.
*/
getFormattedTrackTitle : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_formatted_entry_title, params, callbacks);
},

/*
    Returns all info about specified track in specified playlist.
        Param params.track_id - track ID.
        Param params.playlist_id - playlist ID.
        Param callbacks - see description in AimpManager comments.
    Result - is object with following members: 'id', 'title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'.
*/
getTrackInfo : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_playlist_entry_info, params, callbacks);
},

/*
    Save rating for track in file at AIMP Control plugin work folder.
        Param params.track_id - track ID.
        Param params.playlist_id - playlist ID.
        Param params.rating - track rating in range [0, 5].
        Param callbacks - see description in AimpManager comments.
*/
setTrackRating : function(params, callbacks) {
    this.callRpc(this.aimp_service.set_track_rating, params, callbacks);
},

/*
    Starts playback of specified track in specified playlist.
        Param params.track_id - track ID.
        Param params.playlist_id - playlist ID.
        Param callbacks - see description in AimpManager comments.
    By default start playback current track in current playlist.
*/
play : function(params, callbacks) {
    this.callRpc(this.aimp_service.play, params, callbacks);
},

/*
    Stops playback.
        Param params - empty object.
        Param callbacks - see description in AimpManager comments.
*/
stop : function(params, callbacks) {
    this.callRpc(this.aimp_service.stop, params, callbacks);
},

/*
    Pauses playback.
        Param params - empty object.
        Param callbacks - see description in AimpManager comments.
*/
pause : function(params, callbacks) {
    this.callRpc(this.aimp_service.pause, params, callbacks);
},

/*
    Plays previous track.
        Param params - empty object.
        Param callbacks - see description in AimpManager comments.
*/
playPrevious : function(params, callbacks) {
    this.callRpc(this.aimp_service.play_previous, params, callbacks);
},

/*
    Plays next track.
        Param params - empty object.
        Param callbacks - see description in AimpManager comments.
*/
playNext : function(params, callbacks) {
    this.callRpc(this.aimp_service.play_next, params, callbacks);
},

/*
    Sets/gets specified AIMP status to speciied value.
        Param params.status_id - status ID.
        Param params.value - status value, range depends on status ID.
        Param callbacks - see description in AimpManager comments.
    Supported status IDs and their values ranges:
        status ID          value range
        VOLUME      = 1,  [0,100]
        BALANCE     = 2,
        SPEED       = 3,
        Player      = 4,
        MUTE        = 5,  [0, 1]
        REVERB      = 6,
        ECHO        = 7,
        CHORUS      = 8,
        Flanger     = 9,

        EQ_STS      = 10,
        EQ_SLDR01   = 11,
        EQ_SLDR02   = 12,
        EQ_SLDR03   = 13,
        EQ_SLDR04   = 14,
        EQ_SLDR05   = 15,
        EQ_SLDR06   = 16,
        EQ_SLDR07   = 17,
        EQ_SLDR08   = 18,
        EQ_SLDR09   = 19,
        EQ_SLDR10   = 20,
        EQ_SLDR11   = 21,
        EQ_SLDR12   = 22,
        EQ_SLDR13   = 23,
        EQ_SLDR14   = 24,
        EQ_SLDR15   = 25,
        EQ_SLDR16   = 26,
        EQ_SLDR17   = 27,
        EQ_SLDR18   = 28,

        REPEAT      = 29,  [0, 1]
        ON_STOP     = 30,
        POS         = 31,
        LENGTH      = 32,
        REPEATPLS   = 33,
        REP_PLS_1   = 34,
        KBPS        = 35,
        KHZ         = 36,
        MODE        = 37,
        RADIO       = 38,
        STREAM_TYPE = 39,
        TIMER       = 40,
        SHUFFLE     = 41   [0, 1]
    Returns current value of status if params.value is not specified.
*/
status : function(params, callbacks) {
    this.callRpc(this.aimp_service.status, params, callbacks);
},

/*
    Sets track position.
        Param params.position - int value: seconds from begin. Must be in range [0, track_length_in_seconds).
        Param callbacks - see description in AimpManager comments.
    With empty params returns current position.
*/
trackPosition : function(params, callbacks) {
	var status_params = params.hasOwnProperty('position') ? { status_id : 31,
														      value : params.position
															}
													      : {};
    this.callRpc(this.aimp_service.status, status_params, callbacks);
},

/*
    Sets shuffle playback mode.
        Param params.shuffle_on - string flag: 'on', 'off'.
        Param callbacks - see description in AimpManager comments.
    With empty params returns current mode.
*/
shufflePlaybackMode : function(params, callbacks) {
    this.callRpc(this.aimp_service.shuffle_playback_mode, params, callbacks);
},

/*
    Sets repeat playback mode.
        Param params.repeat_on - string flag: 'on', 'off'.
        Param callbacks - see description in AimpManager comments.
    With empty params returns current mode.
*/
repeatPlaybackMode : function(params, callbacks) {
    this.callRpc(this.aimp_service.repeat_playback_mode, params, callbacks);
},

/*
    Sets volume level.
        Param params.level - volume level.
        Param callbacks - see description in AimpManager comments.
    With empty params returns current volume level.
*/
volume : function(params, callbacks) {
    this.callRpc(this.aimp_service.volume, params, callbacks);
},

/*
    Sets mute playback mode.
        Param params.mute_on - string flag: 'on', 'off'.
        Param callbacks - see description in AimpManager comments.
    With empty params returns current mode.
*/
mute : function(params, callbacks) {
    this.callRpc(this.aimp_service.mute, params, callbacks);
},

/*
    Returns current control playback mode.
        Param params - empty object.
        Param callbacks - see description in AimpManager comments.
    Result is object with following members:
        playback_state - 'playing', 'stopped', 'paused';
		track_position - current track position. Exist only if it has sense.
		track_length - current track length. Exist only if it has sense.
        playlist_id - playlist ID;
        track_id - track ID;
        volume - volume level in range [0-100];
        mute_mode_on - flag of mute mode;
        repeat_mode_on - flag of repeat mode;
        shuffle_mode_on - flag of shuffle mode;
*/
getControlPanelState : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_control_panel_state, params, callbacks);
},

/*
    Returns value not immediately as other functions but as soon as specified event occures.
        Param params.event - event to get notification about.
        Supported events are:
            1) 'play_state_change' - playback state change event (player switch to playing/paused/stopped state)
            Response will contain following members:
                'playback_state', string - playback state (playing, stopped, paused)
				track_position, int - current track position. Exist only if it has sense.
				track_length, int - current track length. Exist only if it has sense.
            2) 'current_track_change' - current track change event (player switched track)
            Response will contain following members:
                1) 'playlist_id', int - playlist ID
                2) 'track_id', int - track ID
            3) 'control_panel_state_change' - one of following events:
                    "playback state, mute, shuffle, repeat, volume level change.
                Response will be the same as get_control_panel_state() function.
            4) 'playlists_content_change' - playlists content change.
    Result is specific for each event.
*/
subscribe : function(params, callbacks) {
    this.callRpc(this.aimp_service.subscribe, params, callbacks);
},

/*
    Returns URI of album cover for specified track in specified playlist.
        Param params.track_id - track ID.
        Param params.playlist_id - playlist ID.
        Param params.cover_width - image width.
        Param params.cover_height - image height.
        Param callbacks - see description in AimpManager comments.
*/
getCover : function(params, callbacks) {
    this.callRpc(this.aimp_service.get_cover, params, callbacks);
}

}; // AimpManager.prototype
