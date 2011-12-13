/*
    Script contains functions for AIMP Control plugin frontend to communicate with AIMP RPC(Xmlrpc/Jsonrpc) server.
    Copyright (c) 2011, Alexey Ivanov
*/

// Global data
var $playlists_tabs = null;
var $playlists_tables = {}; // maps entries table id to datatable object.
var aimp_manager = new AimpManager();
var control_panel_state = {};
var control_menu_state_updaters = {}; // map unique ID of context menu to notifier descriptor(object with following members: notifier - function (entry_control_menu_descriptor), control_menu_descriptor - control menu descritor).
var track_progress_timer = null;
var entries_requests = {}; // contains latest entry list requests to server for each playlist. Need for automatic displaying current track in playlist.

var icon_menu_indicator_opened = 'ui-icon-minus',
	icon_menu_indicator_closed = 'ui-icon-plus';

/* Invoke notifiers for all control menus. */
function syncronizeControlMenus() {
    for (key in control_menu_state_updaters) {
        var notifier_desc = control_menu_state_updaters[key];
        notifier_desc.notifier(notifier_desc.control_menu_descriptor);
    }
}

function getPlaylistDataTable(playlist_id) {
	return $playlists_tables['entries_table_' + playlist_id];
}

function removeHighlightFromAllRows($table) {
	$($table.fnSettings().aoData).each(
		function () {
			$(this.nTr).removeClass('row_selected');
		}
	);
}

function highlightCurrentRow($table, nRow) {
	removeHighlightFromAllRows($table);
	$(nRow).addClass('row_selected');			
}

function getPlaylistIdFromTabId(tab_id) {
	return tab_id.split('_')[1];
}

function getPlaylistIdFromTableId(table_id)
{
	return table_id.split('_')[2]; // get id of playlist HTML table.(Ex.: id = 123 from string like this "entries_table_123" )
}

/* create DataTable control(jQuery plugin) for list of entries. */
function createEntriesControl(index, $tab_ui)
{
    var playlist_id = getPlaylistIdFromTabId($tab_ui.id);
    var $table_with_playlist_id = $('#entries_table_' + playlist_id);

    var $table;

	// Return track desc which linked with string.
	function getDescriptionTrackOfRow(nTr) {
		var playlist_id = getPlaylistIdFromTableId(nTr.parentNode.parentNode.id);
		var row_index = $table.fnGetPosition(nTr);
		if (row_index !== null) {
			var aData = $table.fnGetData(row_index);
			var track_id = aData[0];
			return { playlist_id : parseInt(playlist_id),
					 track_id    : parseInt(track_id)
				   };	
		}
		return null;
	}

    /* Add a click handler to the rows - this could be used as a callback */
    $('#entries_table_' + playlist_id + ' tbody').click(
        function(event) {
			// need to filter out invalid type and non track rows(context menu rows)
			if (   event.target.parentNode !== null
				&& event.target.parentNode.nodeName === 'TR'
				&& $table.fnSettings().aoData.length > 0
				)
			{
				highlightCurrentRow($table, event.target.parentNode);
    			var track_desc = getDescriptionTrackOfRow(event.target.parentNode);
				if (track_desc !== null) {
					if (   track_desc.track_id !== control_panel_state.track_id
						|| track_desc.playlist_id !== control_panel_state.playlist_id
						)
					{
						aimp_manager.play(track_desc,
										  { on_exception : function(error, localized_message) {
															   alert(localized_message);
														   }
										  }); // start playback.
					}
				}	
			}
        }
    );

    $table = $table_with_playlist_id.dataTable( {
        bStateSave : true, // save state in cookies.
        aoColumns : getDataTablesColumnsDescriptors(),
        fnDrawCallback : onPlaylistTableDraw,
        oLanguage : {
            sUrl : getLocalizationDirectory() + '/datatables.txt'
        },
        bServerSide : true,
        sAjaxSource : '', // not used. Instead get data with aimp_manager.getPlaylistEntries() method.
        fnServerData : function (sSource, aoData, fnCallback) {
            // implementation idea is here: http://datatables.net/examples/server_side/pipeline.html

            var on_error = function (error, localized_error_message) {
                var msg = getText('error_playlist_entries_loading')+ ' ' + getText('reason') + ': ' + localized_error_message;
                alert(msg);
            };

            function fnGetKey(aoData, sKey) {
                for (var i = 0, iLen = aoData.length; i < iLen; ++i) {
                    if (aoData[i].name == sKey) {
                        return aoData[i].value;
                    }
                }
                return null;
            }

            var sEcho = fnGetKey(aoData, 'sEcho'); // get value here since it can change when on_success() will be invoked.

            var on_success = function(fnCallback) {
                return function(result) {
                    // call DataTables function - data getter.
                    fnCallback({
                                sEcho : sEcho,
                                iTotalRecords : result.total_entries_count,
                                iTotalDisplayRecords : (result.count_of_found_entries !== undefined) ? result.count_of_found_entries : result.total_entries_count,
                                aaData : result.entries
                    });
                };
            }

			var fields = fnGetKey(aoData, 'sColumns').split(',');
            /* Sort descriptors */
            var order_fields = [];
            var iSortingCols = fnGetKey(aoData, 'iSortingCols');
            for (var i = 0; i < iSortingCols; ++i) {
                var iSortColCurrent = fnGetKey(aoData, 'iSortCol_' + i);
                if ( fnGetKey(aoData, 'bSortable_' + iSortColCurrent) === true) {
                    order_fields.push({
                        field : fields[iSortColCurrent],
                        dir :  fnGetKey(aoData, 'sSortDir_' + i)
                    });
                }
            }

			var request_params = {  playlist_id   : parseInt(playlist_id),
									fields        : fields,
									order_fields  : order_fields,
									start_index   : fnGetKey(aoData, 'iDisplayStart'),
									entries_count : fnGetKey(aoData, 'iDisplayLength'),
									search_string : fnGetKey(aoData, 'sSearch')
								 };
			entries_requests[request_params.playlist_id] = request_params;
            aimp_manager.getPlaylistEntries(request_params,
											{
											  on_success   : on_success(fnCallback),
											  on_exception : on_error,
											  on_complete  : undefined
											}
            );
			
			if (control_panel_state.playlist_id === request_params.playlist_id) {
				gotoCurrentTrackInPlaylist();
			}
        },
        bProcessing : true,
        bJQueryUI : true,
        sPaginationType : 'full_numbers',
        bAutoWidth : false,
		aLengthMenu : [10, 25, 50, 100] // [[10, 25, 50, -1], [10, 25, 50, getText('all_entries')]]
    } );


    $table.fnSettings().aaSorting = []; // disable sorting by 0 column.

    $playlists_tables['entries_table_' + playlist_id] = $table;
}

function gotoCurrentTrackInPlaylist()
{
	if (entries_requests.hasOwnProperty(control_panel_state.playlist_id) ) {
		var request_params = entries_requests[control_panel_state.playlist_id]; // maybe we need copy this
		request_params['track_id'] = control_panel_state.track_id;
		aimp_manager.getEntryPositionInDatatable(request_params,
									 		 { on_success   : function (result) {
															      if (result.page_number >= 0 && result.track_index_on_page >= 0) {
																      gotoCurrentPlaylist(control_panel_state.playlist_id);
																      tryToLocateCurrentTrackInPlaylist(result.page_number, result.track_index_on_page);
																  } else {
																	  //if ( control_panel_state.hasOwnProperty('playlist_id') ) {
																      removeHighlightFromAllRows( getPlaylistDataTable(control_panel_state.playlist_id) );
																	  //}
																  }
															  },
											   on_exception : function(error, localized_message) {
   															      //alert(localized_message);
																  //if ( control_panel_state.hasOwnProperty('playlist_id') ) {
																  removeHighlightFromAllRows( getPlaylistDataTable(control_panel_state.playlist_id) );
																  //}
														      },
											   on_complete  : undefined
											 }
											 );
	}
}

function gotoCurrentPlaylist(playlist_id)
{
	$('div[id*=playlist]', $playlists_tabs).each(function(index, tab_ui) {
		var tab_playlist_id = getPlaylistIdFromTabId(tab_ui.id);
		if (playlist_id == tab_playlist_id) {
			$playlists_tabs.tabs('option', 'selected', -1); // mark all tabs as unselected to force trigger onselect event.
															// Otherwise, in case if we select already selected tab, onselect event will not be triggered.
			$playlists_tabs.tabs('select', index);
			return false;
		}
		return true;
	});
}

function tryToLocateCurrentTrackInPlaylist(entry_page_number, entry_index_on_page)
{
	var $playlist_table = getPlaylistDataTable(control_panel_state.playlist_id);
	var oSettings = $playlist_table.fnSettings();
	var index_of_first_entry_on_page = entry_page_number * oSettings._iDisplayLength;
	if (oSettings._iDisplayStart != index_of_first_entry_on_page) {
		// move to page where current track is visible.
		oSettings._iDisplayStart = index_of_first_entry_on_page;
		oSettings._iDisplayEnd = oSettings._iDisplayStart + oSettings._iDisplayLength;
		$playlist_table.entry_index_on_page_to_highlight_on_update = entry_index_on_page;
		$playlist_table.fnDraw(false);
	} else {
		// highlight current track.
		var dt_row = $(oSettings.aoData).get(entry_index_on_page);
		var nRow = dt_row.nTr;
		highlightCurrentRow($playlist_table, nRow);
	}
}

function onPlaylistTableDraw(oSettings) {
	addControlMenuToEachEntry(oSettings);
	
	var $table = getPlaylistDataTable( getPlaylistIdFromTableId(oSettings.nTable.id) );
	
	if ( $table.hasOwnProperty('entry_index_on_page_to_highlight_on_update') ) {
		highlightCurrentRow($table,
							$table.fnGetNodes($table.entry_index_on_page_to_highlight_on_update)
							);
		delete $table.entry_index_on_page_to_highlight_on_update;
	}
	
	// init all rating widgets in table
	$('div[id^="track_rating_"]', $table).each(function(index, rating_div) {
		initStarRatingWidget(rating_div.id);
	});
}

/* Add control menu switcher and menu itself to all entries. */
function addControlMenuToEachEntry(oSettings)
{
    if (oSettings.aoData.length > 0) { // add control to entry if we have some data. DataTables add string "Nothing found" if there is no data.
        var oTable = $('#' + oSettings.sTableId);
        var $table = $playlists_tables[oSettings.sTableId];

        if ( $('#playcontrol', oTable).length == 0) {
            /*
             Insert a 'entry control menu' column to the table
            */
            var nCloneTh = document.createElement('th');
            nCloneTh.id = 'playcontrol';
            var head_rows = $('thead tr', oTable);
            head_rows.each( function () {
                // insert new column
                added_node = this.insertBefore(nCloneTh.cloneNode(true), this.childNodes[0]);
                // set class of new column equal class of neighbour column. At least one column must exist('title' column is always visible).
                added_node.className = this.childNodes[1].className;
                $(added_node).css('width', '1px'); // set minimal width of column, browser will use content width.
            });
        }

        var nCloneTd = document.createElement('td'); // $('<button class="entry_control_menu_toggle"></button>')
        nCloneTd.innerHTML = '<button class="entry_control_menu_toggle"></button>';
        //nCloneTd.className = "center";

        $('tbody tr', oTable).each( function () {
            this.insertBefore(nCloneTd.cloneNode(true), this.childNodes[0]);
        });

        /*
            Add event listener for opening and closing entry control menu.
            Note that the indicator for showing which row is open is not controlled by DataTables,
            rather it is done here
        */
        $('td .entry_control_menu_toggle', $table.fnGetNodes() ).each( function () {
            $(this).button({
                text: false,
                icons: {
                    primary: icon_menu_indicator_closed
                },
                label: getText('track_contol_menu_open')
            }).click(onContextMenuButtonClick);
        });
    }
}

function getControlMenuDescriptor(nTr)
{
	var playlist_id = getPlaylistIdFromTableId(nTr.parentNode.parentNode.id);
	var $table = getPlaylistDataTable(playlist_id);
	var aData = $table.fnGetData(nTr);
	var entry_id = aData[0];

	var play_button_id          = 'play_entry_' + entry_id;
	var enqueue_entry_button_id = 'enqueue_entry_' + entry_id;
	var remove_from_queue_entry_button_id = 'remove_from_queue_entry_' + entry_id;
	var control_menu_html =   '<table cellpadding="5" cellspacing="0" border="0">'
							+ '<tr>'
							+ '<td><button id="' + play_button_id + '"></button></td>'
							+ '<td><button id="' + enqueue_entry_button_id + '"></button></td>'
							+ '<td><button id="' + remove_from_queue_entry_button_id + '"></button></td>'
							+ '</tr>'
							+ '</table>';

	return {
		html : control_menu_html,
		entry_id : entry_id,
		playlist_id : playlist_id,
		nTr : null,
		play_button_id : play_button_id,
		enqueue_button_id : enqueue_entry_button_id,
		remove_from_queue_button_id : remove_from_queue_entry_button_id
	};
}

function onContextMenuButtonClick() {
	var nTr = this.parentNode.parentNode;
	var entry_control_menu_descriptor = getControlMenuDescriptor(nTr);
	
	var playlist_id = getPlaylistIdFromTableId(nTr.parentNode.parentNode.id);
	var $table = getPlaylistDataTable(playlist_id);
	
	if ( $(this).button('option', 'icons').primary == icon_menu_indicator_opened ) {
		// remove menu updater from global list.
		var notifier_id = entry_control_menu_descriptor.entry_id + '_' + entry_control_menu_descriptor.playlist_id;
		delete control_menu_state_updaters[notifier_id]; // entry_control_menu_descriptor was initialized on menu open.

		/* Control menu for this entry is already open - close it */
		$(this).button('option', {
								   icons: { primary: icon_menu_indicator_closed },
								   label: getText('track_contol_menu_open')
								 }
		);
		$table.fnClose(nTr);
	} else {
		/* Open control menu for this entry */
		$(this).button('option', {
								   icons: { primary: icon_menu_indicator_opened },
								   label: getText('track_contol_menu_close')
								 }
		);
		entry_control_menu_descriptor.nTr = $table.fnOpen(nTr, entry_control_menu_descriptor.html, 'entry_control_menu');

		initTrackControlMenu(entry_control_menu_descriptor);
		updateTrackControlMenu(entry_control_menu_descriptor);
		/*
			Use details of DataTables implementation to correct work of $table.fnClose() function:
			if DataTable bServerSide flag is set, $table.fnOpen() will not add new row in aoOpenRows array.
		*/
		$table.fnSettings().aoOpenRows.push({
			'nTr': entry_control_menu_descriptor.nTr,
			'nParent': nTr
		});
	}
}

/* Returns true if playback is active now. */
function isPlaybackActive() {
    return control_panel_state.playback_state == 'playing';
}

/*
    Returns true if track in control_menu_descriptor is current track in AIMP.
    Notice, it may not be played currently, use isCurrentTrackPlaying() instead if needed.
*/
function isCurrentTrackActive(control_menu_descriptor) {
    return control_panel_state.playlist_id == control_menu_descriptor.playlist_id
           && control_panel_state.track_id == control_menu_descriptor.entry_id
    ;
}

/* Returns true if track in control_menu_descriptor is played now. */
function isCurrentTrackPlaying(control_menu_descriptor) {
    return isPlaybackActive()
           && isCurrentTrackActive(control_menu_descriptor)
    ;
}

/* Init controls of created entry menu. */
function initTrackControlMenu(control_menu_descriptor)
{
    var on_control_menu_command = function(error, localized_message) {
        alert(localized_message);
    };

    var play_button = $('#' + control_menu_descriptor.play_button_id, control_menu_descriptor.nTr);
    play_button.button({
        text : false
    }).click(function() {
        if ( !isCurrentTrackPlaying(control_menu_descriptor) ) {
            // if current track is active proceed playing, do not start track from beginning.
            var play_args = isCurrentTrackActive(control_menu_descriptor)
                            ? {}
                            : { track_id    : parseInt(control_menu_descriptor.entry_id),
                                playlist_id : parseInt(control_menu_descriptor.playlist_id)
                              }
            ;
            aimp_manager.play(play_args,
                              { on_exception : on_control_menu_command }
							  ); // start playback.
        } else {
            aimp_manager.pause({}, { on_exception : on_control_menu_command }); // pause playback
        }
    });

    var enqueue_button = $('#' + control_menu_descriptor.enqueue_button_id, control_menu_descriptor.nTr);
    enqueue_button.button({
        text : false,
		icons: { primary: 'ui-icon-circlesmall-plus' },
		label : getText('track_contol_menu_enqueue')		
    }).click(function() {
		var args = { track_id    : parseInt(control_menu_descriptor.entry_id),
					 playlist_id : parseInt(control_menu_descriptor.playlist_id)
				   }
		aimp_manager.enqueueTrack(args,
								  { on_exception : on_control_menu_command }
								  );
    });

    var remove_from_queue_button = $('#' + control_menu_descriptor.remove_from_queue_button_id , control_menu_descriptor.nTr);
    remove_from_queue_button.button({
        text : false,
		icons: { primary: 'ui-icon-circlesmall-minus' },
		label : getText('track_contol_menu_remove_from_queue')
    }).click(function() {
		var args = { track_id    : parseInt(control_menu_descriptor.entry_id),
					 playlist_id : parseInt(control_menu_descriptor.playlist_id)
				   }
		aimp_manager.removeTrackFromPlayQueue(args,
									          { on_exception : on_control_menu_command }
											  );
    });

    // add updater to global list.
    var notifier_id = control_menu_descriptor.entry_id + '_' + control_menu_descriptor.playlist_id;
    control_menu_state_updaters[notifier_id] = { notifier : updateTrackControlMenu, control_menu_descriptor : control_menu_descriptor };
}

function updateTrackControlMenu(control_menu_descriptor) {
    // play/pause button
    var play_button = $('#' + control_menu_descriptor.play_button_id, control_menu_descriptor.nTr);
    var current_track_active = isCurrentTrackPlaying(control_menu_descriptor);
    $(play_button).button('option', {
                                        icons: {
                                            primary: current_track_active ? 'ui-icon-pause' : 'ui-icon-play'
                                        },
                                        label : getText(current_track_active ? 'track_contol_menu_pause' : 'track_contol_menu_play')
                                    }
    );
}

/* Returns array of descriptors for DataTables object. */
function getDataTablesColumnsDescriptors()
{
    var entry_fields = getDisplayedEntryFieldsFromCookie();
    var fields_descriptions = getEntryFieldsDescriptions();
    var columns_desc = [ { sName : 'id', bVisible : false } ]; // first field always id field, it has no caption and invisible
    for (field_index in entry_fields) {
        var field_settings = {  sName : entry_fields[field_index],
                                sTitle : fields_descriptions[entry_fields[field_index]]
        };

        if (entry_fields[field_index] == 'duration') { // use spec format of entry duration field.
            initDurationField(field_settings);
        } else if (entry_fields[field_index] == 'filesize') {
            initFileSizeField(field_settings);
        } else if (entry_fields[field_index] == 'bitrate') {
            initBitrateField(field_settings);
        } else if (entry_fields[field_index] == 'rating') {
            initRatingField(field_settings);
		}
        columns_desc.push( field_settings );
    }
    return columns_desc;
}

function initDurationField(field_settings) {
	/*
		oObj members:
			"iDataRow": iRow,
			"iDataColumn": iColumn,
			"aData": oSettings.aoData[iRow]._aData,
			"oSettings": oSettings
	*/
    field_settings.fnRender = function ( oObj ) {
        var time_ms = oObj.aData[oObj.iDataColumn];
        return formatTime(time_ms);
    }
    // align in cell center.
    field_settings.sClass = 'center';
}

function initFileSizeField(field_settings) {
    field_settings.fnRender = function ( oObj ) {
        var size_in_bytes = oObj.aData[oObj.iDataColumn];
        return formatFileSize(size_in_bytes);
    }
    // align in cell center.
    field_settings.sClass = 'center';
}

function initBitrateField(field_settings) {
	// display units only in table header.
    field_settings.sTitle = field_settings.sTitle + ', ' + getText('kilobits_per_second');

    // align in cell center.
    field_settings.sClass = 'center';
}

function initRatingField(field_settings) {
    field_settings.fnRender = function ( oObj ) {
        var aimp_rating = oObj.aData[oObj.iDataColumn];
        var playlist_id = getPlaylistIdFromTableId(oObj.oSettings.sTableId);
		var track_id = oObj.aData[0];
		var div_id = 'track_rating_' + playlist_id + '_' + track_id;

		var html = '<div id="' + div_id + '">';
		for (var i = 1; i <= 5; i++) {
			html += '<input type="radio" class="rating_star" value="' + i + '"'
					+ (i == aimp_rating ? ' checked="true"' : '')
					+ '/>';
		}
		html += '</div>';
		return html;
    }
    // align in cell center.
    field_settings.sClass = 'center';
	field_settings.sWidth = (16 * 6) + 'px';
}

/* Deletes all playlists controls(jQuery UI Tabs) */
function deletePlaylistsControls()
{
    if ($playlists_tabs !== null) {
        $playlists_tables = {}; // clear
        $('#playlists > div').remove();
        $playlists_tabs.tabs('destroy'); // if tabs control is already created - destroy all tabs.
		$playlists_tabs = null;
    }
}

function isPlaylistContentLoaded($tab_ui) {
	var playlist_id = getPlaylistIdFromTabId($tab_ui.id);
	return $playlists_tables['entries_table_' + playlist_id] !== undefined;
}

/* create controls(jQuery UI Tabs) for list of playlists. */
function createPlaylistsControls(playlists)
{
    if ($playlists_tabs === null) {
        $playlists_tabs = $('#playlists').tabs({
            cookie: { expires: 1 }, // store cookie for a day, without, it would be a session cookie
			select: function(event, $ui) { // load content of playlist on tab activation, if content is not loaded yet.
						var $tab_ui = $ui.panel;
						if ( !isPlaylistContentLoaded($tab_ui) ) {
							createEntriesControl($ui.index, $tab_ui);	
						}
					}
        }); // necessary initialization of Tabs control.
    }

    // create tabs for each playlist.
    for(i = 0; i < playlists.length; ++i) {
        var playlist_id_name = 'playlist_' + playlists[i].id;
        // we need to have unique ID to attach div to tab.
        var div_html = '<div id="' + playlist_id_name + '">' + createTemplateEntriesTable(playlists[i].id) + '</div>';
        $(div_html).appendTo('body');
        $playlists_tabs.tabs('add',
							 '#' + playlist_id_name,
		 					 playlists[i].title
							 );
    }
	
	if (   playlists.length > 1 // here we want trigger onselect event of playlist tab. But on 1 tab event will not be tringgered, so load playlist content manually
		&& control_panel_state.hasOwnProperty('playlist_id') ) {
		// content will be loaded on selection.
		gotoCurrentPlaylist(control_panel_state.playlist_id);
	} else {
	    // select all created playlists and init them.
		$('#playlists > div').each(createEntriesControl);
	}
}

/*
    Loads list of playlists form server and create tabs for each playlist into div with id 'playlists'.
    Each playlist use ajax request to get tracks(playlist entries).
*/
function loadPlaylists()
{
    var on_error = function (error, localized_error_message) {
        var msg = getText('error_playlists_loading')+ ' ' + getText('reason') + ': ' + localized_error_message;
        var $playlists_obj = $('#playlists');
        $('ul', $playlists_obj).append(msg);
        $playlists_obj.tabs({});
        return true; // means that we handled error.
    };

    aimp_manager.getPlaylists({ fields : ['id', 'title'] },
                              {
                                on_success : createPlaylistsControls,
                                on_exception : on_error,
                                on_complete : undefined
                              }
    );
}

/* Init control panel controls */
function initControlPanel()
{
    // common error handler for control panel actions.
    var on_control_panel_command = function(error, localized_message) {
        alert(localized_message);
    };

    // Note: all dynamic button's icons and labels will be set in updateControlPanel();

    $('#play').button({
        text: false
    }).click(function() {
        if ( isPlaybackActive() ) {
            aimp_manager.pause({}, { on_exception : on_control_panel_command }); // pause playback
        } else {
            aimp_manager.play({}, { on_exception : on_control_panel_command }); // start playback.
        }
    });

    $('#stop').button({
        text: false,
        icons: { primary: 'ui-icon-stop' },
        label: getText('control_panel_stop')
    }).click(function() {
        aimp_manager.stop({}, { on_exception : on_control_panel_command }); // stop playback
    });

    $('#previous').button({
        text: false,
        icons: { primary: 'ui-icon-triangle-1-w' },
        label: getText('control_panel_previous')
    }).click(function() {
        aimp_manager.playPrevious({}, { on_exception : on_control_panel_command }); // play previous track
    });

    $('#next').button({
        text: false,
        icons: { primary: 'ui-icon-triangle-1-e' },
        label : getText('control_panel_next')
    }).click(function() {
        aimp_manager.playNext({}, { on_exception : on_control_panel_command }); // play next track
    });

    $('#shuffle').button({
        text: false,
        icons: { primary: 'ui-icon-shuffle' }
    }).click(function() {
        aimp_manager.shufflePlaybackMode({ shuffle_on : !control_panel_state.shuffle_mode_on },
                                         { on_exception : on_control_panel_command }
        ); // activate/deactivate shuffle playback mode.
    });

    $('#repeat').button({
        text: false,
        icons: { primary: 'ui-icon-refresh' }
    }).click(function() {
        aimp_manager.repeatPlaybackMode({ repeat_on : !control_panel_state.repeat_mode_on },
                                        { on_exception : on_control_panel_command }
        ); // activate/deactivate repeat playback mode.
    });

    $('#show_settings_form').button({
        text: false,
        icons: { primary: 'ui-icon-wrench' },
        label : getText('control_panel_settings')
    }).click(
        function() {
            $('#settings-dialog-form').dialog('open');
        }
    );

    var mute_hover_handler = function () {
        $(this).toggleClass('ui-state-hover');
    }
    $('#mute_button').click(function () {
        aimp_manager.mute({ mute_on : !control_panel_state.mute_mode_on },
                          { on_exception : on_control_panel_command }
        ); // activate/deactivate mute mode.
    }).hover(mute_hover_handler, mute_hover_handler);

    $('#volume_slider').slider({
        value: control_panel_state.volume,
        min : 0,
        max : 100,
        orientation: 'horizontal',
        range: 'min',
        animate: true,
        stop: function(event, ui) {
            aimp_manager.volume({ level : ui.value },
                                { on_exception : on_control_panel_command }
            ); // set volume
        }
    });

    $('#control_panel_buttons').buttonset();
}

/* Update control panel page controls according to Aimp state. */
function updateControlPanel()
{
    var control_panel = control_panel_state; // used for easy debug in Dragonfly

    var play_button = $('#play');
    if (play_button[0].className == '') { // on first call we need to init all controls.
        initControlPanel();
    }

    // play/pause button
    var playing_now = isPlaybackActive();
    play_button.button('option', {
                                    icons: {
                                        primary: playing_now ? 'ui-icon-pause' : 'ui-icon-play'
                                    },
                                    label: getText(playing_now ? 'control_panel_pause' : 'control_panel_playback')
                                 }
    );

    // mute button
    var mute_button = $('#mute_button');
    var mute_icon = $('#mute_icon');
    if ( control_panel.mute_mode_on ) {
        mute_icon.removeClass('ui-icon-volume-on');
        mute_icon.addClass('ui-icon-volume-off');
        mute_button.attr( 'title', getText('control_panel_mute_off') );
    } else {
        mute_icon.removeClass('ui-icon-volume-off');
        mute_icon.addClass('ui-icon-volume-on');
        mute_button.attr( 'title', getText('control_panel_mute_on') );
    }

    // volume slider
    var volume_slider = $('#volume_slider');
    volume_slider.slider('value', control_panel.volume);
	volume_slider.attr('title',
					   getText('control_panel_volume') + ' ' + control_panel.volume + '/100'
                       );

    // shuffle button
    var shuffle_button = $('#shuffle');
    shuffle_button.button('option', {
                                        label: getText(control_panel.shuffle_mode_on ? 'control_panel_shuffle_off' : 'control_panel_shuffle_on')
                                    }
    );
    shuffle_button.attr('checked', control_panel.shuffle_mode_on);
    shuffle_button.button('refresh');

    // repeat button
    var repeat_button = $('#repeat');
    repeat_button.button('option',  {
                                        label: getText(control_panel.repeat_mode_on ? 'control_panel_repeat_off' : 'control_panel_repeat_on')
                                    }
    );
    repeat_button.attr('checked', control_panel.repeat_mode_on);
    repeat_button.button('refresh');
}

/*
    Updates state of Control Panel controls to be sync with Aimp.
*/
function updateControlPanelState(result) {
    control_panel_state = result; // update global variable.
    updateControlPanel();
    syncronizeControlMenus();
	gotoCurrentTrackInPlaylist();
};

function getFormatOfTrackInfoFromCookies()
{
    var track_info_format_string = $.cookie('track-info-format-string');
    if (track_info_format_string !== null) {
        return track_info_format_string;
    }
	return '%IF(%a,%a - %T,%T)';
}

/* Updates state of Playback(current playback state and scrolling track info) Panel controls to be sync with Aimp. */
function updatePlaybackPanelState(control_panel_state)
{
    var $scroll_text_div = $('#scroll_current_track_container > div');
    if ( isPlaybackActive() ) {
        $('#playback_state_label').text( getText('playback_state_playing') );

        aimp_manager.getFormattedTrackTitle({
                                              track_id : control_panel_state.track_id,
                                              playlist_id : control_panel_state.playlist_id,
                                              format_string : getFormatOfTrackInfoFromCookies()
                                            },
                                            { on_success : function (result) {
                                                            //alert(result.formatted_string);
                                                            $scroll_text_div.text(result.formatted_string);
                                                           }

                                            }
        );
    } else {
        $('#playback_state_label').text( getText('playback_state_stopped') );
        $scroll_text_div.text('');
    }
	
	updateTrackProgressBarState(control_panel_state);
}

/* Updates state track progress bar controls to be sync with Aimp. */
function updateTrackProgressBarState(control_panel_state)
{
    var $track_progress_bar = $('#track_progress_bar');
	
	if ($track_progress_bar[0].className == '') { // first time initialization.
		$track_progress_bar.slider({ min : 0,
								     max : control_panel_state.track_length,
									 value : control_panel_state.track_position,
									 animate : true,
								  	 stop : function(event, ui) {
									 	 aimp_manager.trackPosition({ position : ui.value },
																	{ on_exception : function(error, localized_message) {
																						 alert(localized_message);
																					 }
																	}
										 ); // set track position
									 }
								   });
	}
	
	if ( control_panel_state.hasOwnProperty('track_length') ) {
		setTrackProgressBarState($track_progress_bar, 'enabled');
		
		$track_progress_bar.slider('option', 'value', control_panel_state.track_position);
		$track_progress_bar.slider('option', 'max',   control_panel_state.track_length);
		
		updateTrackProgressBarHintText($track_progress_bar);
	} else {
		setTrackProgressBarState($track_progress_bar, 'disabled');
	}
}

// Activates or deactivates track progress bar.
function setTrackProgressBarState($track_progress_bar, state) {
	var enabled = (state === 'enabled');
	$('.ui-slider-handle', $track_progress_bar).css('visibility', enabled ? 'visible': 'hidden'); // we need to hide progress pointer, since progress has no sense.
	$track_progress_bar.slider('option', 'disabled', !enabled);
	
	if (track_progress_timer !== null) {
		window.clearInterval(track_progress_timer);
	}
	
	if (enabled) {
		if ( isPlaybackActive() ) {
			var refresh_time_ms = 1000;
			track_progress_timer = window.setInterval(function () {
														var old_value_sec = $track_progress_bar.slider('option', 'value');
														var new_value_sec = old_value_sec + refresh_time_ms / 1000;
														$track_progress_bar.slider('option', 'value', new_value_sec);
														updateTrackProgressBarHintText($track_progress_bar);
													  },
													  refresh_time_ms
													  );
		}
	} else {
		$track_progress_bar.attr('title', '');
	}
}

function updateTrackProgressBarHintText($track_progress_bar) {
	var progress_sec = $track_progress_bar.slider('option', 'value');
	var length_sec = $track_progress_bar.slider('option', 'max');
	$track_progress_bar.attr('title',
							 formatTime(progress_sec * 1000) // represent seconds as milliseconds.
							 + '/'
							 + formatTime(length_sec * 1000) // represent seconds as milliseconds.
							 );	
}

/*
    Subscribe for control panel change event.
    Endless cycle, call only once at page loading.
*/
function subscribeOnControlPanelChangeEvent() {

    aimp_manager.subscribe( { event : 'control_panel_state_change' },
                            {   on_success : function(result) {
                                    updateControlPanelState(result);
                                    updatePlaybackPanelState(result);

                                    // update track info dialog content if dialog is opened.
                                    if ( isPlaybackActive() ) {
                                        var $track_info_dialog = $('#track_info_dialog');
                                        if ( $track_info_dialog.dialog('isOpen') ) {
                                            var dialog_position = $track_info_dialog.dialog('option', 'position');
                                            //$('#playback_panel_container').click();
                                            updateTrackInfoDialogContent({
                                                                          callback: function (point) {
                                                                            showTrackInfoDialogAtCoords(point.x, point.y);
                                                                          },
                                                                          param: { x: dialog_position[0], y: dialog_position[1] }
                                                                         });
                                        }
                                    }
                                },
                                on_exception : function(error, message) {
                                    alert(message + ', error code = ' + error.code);
                                }, // nothing to do in on_exception(), just try to subscribe again, on_complete() do it.
                                on_complete : subscribeOnControlPanelChangeEvent // will be called unconditionally.
                            }
    );
}

/*
    Subscribe for forced track position change event.
    Endless cycle, call only once at page loading.
*/
function subscribeOnTrackPositionChangeEvent() {

    aimp_manager.subscribe( { event : 'play_state_change' },
                            {   on_success : function(result) {                                   
                                    updateTrackProgressBarState(result);
                                },
                                on_exception : function(error, message) {
                                    alert(message + ', error code = ' + error.code);
                                }, // nothing to do in on_exception(), just try to subscribe again, on_complete() do it.
                                on_complete : subscribeOnTrackPositionChangeEvent // will be called unconditionally.
                            }
    );
}

/*
    Subscribe for playlists content change event.
    Endless cycle, call only once at page loading.
*/
function subscribeOnPlaylistsContentChangeEvent() {

    aimp_manager.subscribe( { event : 'playlists_content_change' },
                            {   on_success : function(result) {
                                    if (result.playlists_changed) {
                                        deletePlaylistsControls();
                                        loadPlaylists();
                                    }
                                },
                                on_exception : function(error, message) {
                                    alert(message + ', error code = ' + error.code);
                                }, // nothing to do in on_exception(), just try to subscribe again, on_complete() do it.
                                on_complete : subscribeOnPlaylistsContentChangeEvent // will be called unconditionally.
                            }
    );
}

/* Updates Aimp control panel state. Sync it with Aimp. */
function syncronizeControlPanelStateWithAimp()
{
    var on_error = function (error, localized_error_message) {
        var msg = getText('error_get_control_panel_state')+ ' ' + getText('reason') + ': ' + localized_error_message;
        alert(msg);
        return true; // means that we handled error.
    };

    aimp_manager.getControlPanelState(  {},
                                        {
                                          on_success : function(result) {
                                            updateControlPanelState(result);
                                            updatePlaybackPanelState(result);
                                          },
                                          on_exception : on_error,
                                          on_complete : undefined
                                        }
    );
}

/* Initialize all page controls. Function is called on after page loading.*/
function initAimpControlPage()
{
    localizePage();

    initTrackInfoDialog(); // this should be called before subscribing to syncronization with AIMP.
    subscribeOnControlPanelChangeEvent();
    syncronizeControlPanelStateWithAimp();
    subscribeOnPlaylistsContentChangeEvent();
	subscribeOnTrackPositionChangeEvent(); ///!!! this can be done on track playing start.
    initSettingsDialog();
    loadPlaylists();
}

function initTrackInfoDialog()
{
    //$.fx.speeds._default = 1000;
    $('#track_info_dialog').dialog({
        autoOpen : false,
        minWidth : 300,
        width : 300
        //show: 'blind'
        //hide: 'explode'
    });
    $('#playback_panel_container').click(function(event) {
        updateTrackInfoDialogContent({
                                      callback: function (point) {
                                        if ( isPlaybackActive() ) {
                                            showTrackInfoDialogAtCoords(point.x, point.y);
                                        }
                                      },
                                      param: { x: event.pageX, y: event.pageY }
                                     }
        );
        return false;
    });
	
	initStarRatingWidget('track_info_rating');
}

function onRatingWidgetClick(value, link) {
	var rating_div = this.parentNode;
	if (rating_div === undefined) {
		return;
	}
	var parts = rating_div.id.split('_');
	var playlist_id = parts.length > 2 ? parseInt(parts[2]) : control_panel_state.playlist_id;
	var track_id = parts.length > 3 ? parseInt(parts[3]) : track_desc.playlist_id;
	
	aimp_manager.setTrackRating({
								  track_id: track_id,
								  playlist_id: playlist_id,
								  rating: (value !== undefined ? parseInt(value) // value range is [1, 5].
															   : 0 // set 0 rating, for AIMP it means "rating is not set".
										  )
								},
								{
								  on_success : undefined,
								  on_exception : undefined,
								  on_complete : undefined
								}
	);
}

/*
	Init star rating widget.
	Returns rating widget.
*/
function initStarRatingWidget(div_id)
{
    return $('#' + div_id + ' .rating_star').rating({
        callback: onRatingWidgetClick,
        cancel: getText('track_info_dialog_cancel_rating'),
        cancelValue: 0
    });
}

function setRatingWidgetValue(div_id, value)
{
	$('#' + div_id + ' .rating_star').rating('select',
											 value,
											 false // do not invoke callback on 'select'
                                             );
}

function resetRatingWidgetValue(div_id)
{
	$('#' + div_id + ' .rating_star').rating('drain');
}

/* Shows dialog in position (coordX, coordY) on page. */
function showTrackInfoDialogAtCoords(coordX, coordY)
{
    var $track_info_dialog = $('#track_info_dialog');
    $track_info_dialog.dialog('option', 'position', [coordX, coordY]);
    $track_info_dialog.dialog('open');
}

/*
    Update content of track info dialog.
    On success calls on_success_callback_descriptor.callback function with param on_success_callback_descriptor.param.
*/
function updateTrackInfoDialogContent(on_success_callback_descriptor) {
    aimp_manager.getTrackInfo(  {
                                  track_id : control_panel_state.track_id,
                                  playlist_id : control_panel_state.playlist_id
                                },
                                {
                                  on_success : function(track_info) {
                                    fillTrackInfoTable(track_info);
                                    on_success_callback_descriptor.callback(on_success_callback_descriptor.param)
                                  },
                                  on_exception : undefined,
                                  on_complete : undefined
                                }
    );
}

/* Fill table with info about track. */
function fillTrackInfoTable(track_info)
{
    function setFieldText(track_field)
    {
        $('#track_info_' + track_field).text(track_info[track_field]);
    }

    var text_track_fields = getSupportedTrackFields();

    var filesize_field = 'filesize';
    removeElementFromArrayByValue(text_track_fields, filesize_field);
    $('#track_info_' + filesize_field).text( formatFileSize(track_info[filesize_field]) );

    var bitrate_field = 'bitrate';
    removeElementFromArrayByValue(text_track_fields, bitrate_field);
    $('#track_info_' + bitrate_field).text( track_info[bitrate_field] + ' ' +getText('kilobits_per_second') );

    var duration_field = 'duration';
    removeElementFromArrayByValue(text_track_fields, duration_field);
    $('#track_info_' + duration_field).text( formatTime(track_info[duration_field]) );

    var rating_field = 'rating';
    removeElementFromArrayByValue(text_track_fields, rating_field);
    var rating_value = track_info[rating_field];
    if (0 <= rating_value && rating_value <= 5) {
        if (rating_value > 0) {
            // set rating.
			setRatingWidgetValue('track_info_rating', rating_value - 1); // AIMP rating is in range [0(not set), 5(max rating)]. But we must use range [0, 4] for this control.
        } else {
			resetRatingWidgetValue('track_info_rating');
        }
    }

    // text fields without processing.
    for (field in text_track_fields) {
        setFieldText(text_track_fields[field]);
    }
}

/* Init static l10n string on page here. */
function localizePage()
{
    // init page title
    document.title = getText('page_title');

    // init settings dialog
    $('#settings-dialog-form').attr( 'title', getText('settings_dialog_title') );
    $('#settings-dialog-tab-playlistview-label').text( getText('settings_dialog_playlist_view_tab_title') );
    $('#settings-dialog-tab-language-label').text( getText('settings_dialog_language_tab_title') );
	$('#settings-dialog-tab-trackinfo-label').text( getText('settings_dialog_trackinfo_tab_title') );

    $('#entry_fields_label_show').text( getText('settings_dialog_playlist_view_tab_show_fields') );
    $('#entry_fields_label_hide').text( getText('settings_dialog_playlist_view_tab_hide_fields') );

	$('#track-title-format').text( getText('settings_dialog_trackinfo_format') );

    localizeTrackInfoDialog();
}

/* Fill labels of track info dialog. */
function localizeTrackInfoDialog()
{
    // dialog caption
    $('#track_info_dialog').attr( 'title', getText('track_info_dialog_caption') );

    // field labels
    function setLabelText(track_field)
    {
        $('#label_track_info_' + track_field).text( getText('entry_field_caption_' + track_field) + ': ' );
    }

    var all_track_fields = getSupportedTrackFields();
    for (field in all_track_fields) {
        setLabelText(all_track_fields[field]);
    }
}

/* Returns array of entry fields. */
function getDisplayedEntryFieldsFromCookie()
{
    var entry_fields_string = $.cookie('view-entry-fields');
    if (entry_fields_string == null) {
        return ['title']; // by default return title.
    }

    return entry_fields_string.split(',');
}

/*
    Returns HTML code of table of playlist entries.
    Table has id='entries_table_' + playlist_id
*/
function createTemplateEntriesTable(playlist_id)
{
    var table_columns = ['id'].concat( getDisplayedEntryFieldsFromCookie() ); // first column always id
    var table_html = '\
    <table id="entries_table_' + playlist_id + '" class="display" cellpadding="0" cellspacing="0" border="0" width="100%">\
    <thead>\
        <tr>'
    +
        array_join('<th>', table_columns, '</th>')
    +
    '\
        </tr>\
    </thead>\
    <tbody></tbody>\
    </table>';
//    <tfoot>\
//        <tr>'
//    +
//        array_join('<th>', table_columns, '</th>')
//    +
//    '\
//        </tr>\
//    </tfoot>';

    return table_html;
}

/*
    Joins array of strings in string like this: "'left_part'element'right_part'".
    Example:
            Arguments:
                left_part = '<th>'
                array = new Array('1', '2')
                right_part = '</th>'
            Result: '<th>1</th><th>2</th>'
*/
function array_join(left_part, array, right_part)
{
    return left_part + array.join(right_part + left_part) + right_part;
}

/* Returns string array of entry fields. */
function getSupportedTrackFields()
{
    return new Array('title', 'artist', 'album', 'date', 'genre', 'bitrate', 'duration', 'filesize', 'rating'); //, 'albumcover'
}

/* Remove first equal element from array. */
function removeElementFromArrayByValue(array, value)
{
    for (index in array) {
        if (array[index] === value) {
            array.splice(index, 1);
            return;
        }
    }
}

/*
    Represents time in milliseconds as 'hour : minute : second'.
*/
function formatTime(input_time_ms) {
    var input_time_sec = input_time_ms / 1000;

    var time_hour = (input_time_sec / 3600) | 0;
    var time_min = ( (input_time_sec % 3600) / 60 ) | 0;
    var time_sec = (input_time_sec % 60) | 0;

    function padout(number) {
        return (number < 10) ? '0' + number : number;
    }

    var delimeter = ':';

    return  (
             (time_hour > 0) ? (padout(time_hour) + delimeter)
                             : ''
            ) // show hours if hour is non-zero.
            + padout(time_min)
            + delimeter
            + padout(time_sec);
}

/* Represents file size in bytes as MB or KB. */
function formatFileSize(size_in_bytes) {
    var megabyte = 1048576;
    if (size_in_bytes > megabyte) {
        return (size_in_bytes / megabyte).toFixed(2) + ' MB';
    } else {
        return (size_in_bytes / 1024).toFixed(2) + ' KB';
    }
}