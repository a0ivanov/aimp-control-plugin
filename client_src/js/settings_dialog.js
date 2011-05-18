/*
    Script contains settings dialog implementation for  AIMP Control plugin frontend.
    Copyright (c) 2011, Alexey Ivanov
*/

// Initialize tab 'playlist view' and return function which saves data from this tab in cookies.
function createTabPlaylistView()
{
    createSettingsFieldsListsControls();
    
    var $fields_displayed = $('#fields_displayed');
    var $fields_hidden = $('#fields_hidden');
    
    // if we have only one displayed field we prevent it from dragging to hidden field list.
    var preventLastDisplayedListFromHiding = function(event, ui) {
        var $fields = $('#fields_displayed > li');
        if ($fields.length == 1) {
            $fields.addClass('ui-state-disabled');
        }
    }
    
    preventLastDisplayedListFromHiding(null, null);
    
    $fields_displayed.sortable({
        connectWith: '.connectedSortableFields',
        cancel: '.ui-state-disabled',
        receive: function(event, ui) { // if some fields were added to displayed list so make them all to be enable to hide.
                     var $fields = $('#fields_displayed > li');
                     if ($fields.length > 1) {
                         $fields.removeClass('ui-state-disabled');
                     }                 
                 }          
    });
    
    $fields_hidden.sortable({
        connectWith: '.connectedSortableFields',
        receive: preventLastDisplayedListFromHiding     
    });
    
    $("#fiels_displayed, #fields_hidden").disableSelection();
    
    // a workaround for a flaw in the demo system (http://dev.jqueryui.com/ticket/4375), ignore!
    $("#dialog").dialog("destroy");
    
    var allFields = $([]).add($fields_displayed).add($fields_hidden);
    
    var saveTabData = function () {
        // save cookie here
        $.cookie('view-entry-fields',
                 $fields_displayed.sortable('toArray').join(',').replace(/view-entry-fields_/g, ''), // remove all occurences of 'view-entry-fields_' from string.
                 { expires: 365 /*in days*/ }
                 );
    }
    
    return saveTabData
}

// Initialize tab 'language' and return function which saves data from this tab in cookies.
function createTabLanguage() {
    var $lang_combo = $('#language-combobox');
    $lang_combo.selectOptions(getLanguageID());

    var saveTabData = function () {
        var new_lang_id = $lang_combo.selectedOptions().val();
        // save cookie here
        $.cookie('language',
                 new_lang_id,
                 { expires: 365 /*in days*/ }
                 );
    }
    
    return saveTabData; 
}

function createTabTrackInfo() {
    var $format_input = $('#track-format-input');
    $format_input.val( getFormatOfTrackInfoFromCookies() );

    var saveTabData = function () {
        var new_format = $format_input.val();
        // save cookie here
        $.cookie('track-info-format-string',
                 new_format,
                 { expires: 365 /*in days*/ }
                 );
    }
    
    return saveTabData; 
}

// Creates settings dialog.
function initSettingsDialog()
{
    $('#settings-dialog-tabs').tabs();
    
    // create tabs content. If you need to add one more settings tab, just provide createTabXXX() method and push save function to to tabsSaveFunctions.
    var tabsSaveFunctions = [];
    tabsSaveFunctions.push( createTabPlaylistView() );
    tabsSaveFunctions.push( createTabLanguage() );
    tabsSaveFunctions.push( createTabTrackInfo() );
    
    // create dialog.
    var dialog_form = $("#settings-dialog-form").dialog({
        autoOpen: false,
        height: 570,
        width: 445,
        modal: true,
        // buttons: {}, // add buttons below since we can't pass localized text as object member here.
        close: function() {
        }
    });
    
    // add dialog's buttons.
    var dialog_buttons = {};
    // save button
    dialog_buttons[getText('settings_dialog_save_button')] = function() {
        // save data of all tabs here.
        for (i=0; i < tabsSaveFunctions.length; ++i) {
            tabsSaveFunctions[i]();
            location.reload(); // reload page to apply changes.
        } 
        $(this).dialog('close');
    };
    // cancel button
    dialog_buttons[getText('settings_dialog_cancel_button')] = function() {
        $(this).dialog('close');
    };    
    
    dialog_form.dialog('option', 'buttons', dialog_buttons);          
}

/*
    Returns difference between two arrays.
        Result array contains:
        1) elements from 1st array that are absent in 2nd array.
        2) elements from 2nd array that are absent in 1st array.
*/
function getArrayDifference(array1, array2)
{
    var diff = [];
    for (index1 in array1) {
        if ( jQuery.inArray(array1[index1], array2) == -1 ) { // array2.indexOf(array1[index1])
            diff.push(array1[index1]);
        }
    }
    for (index2 in array2) {
        if ( jQuery.inArray(array2[index2], array1) == -1 ) { // array1.indexOf(array2[index2])
            diff.push(array2[index2]);
        }
    }
    return diff;
}

/* Returns entry fields description object: associative array(keys are fields, values are captions(to show it to user)) */
function getEntryFieldsDescriptions()
{
    var fields_names = getSupportedTrackFields();
    
    var description = {};
    for (field_index in fields_names) {
        var field_name = fields_names[field_index];
        description[ field_name ] = getText('entry_field_caption_' + field_name);
    }
    return description;
}

/* Creates DOM controls that represent list of visible and hidden fields. */
function createSettingsFieldsListsControls()
{
    // create displayed controls
    var fields_description = getEntryFieldsDescriptions();
    var fields_displayed = getDisplayedEntryFieldsFromCookie();
    
    var $ui_displayed = $('#fields_displayed');
    var items_html = '';
    for (index in fields_displayed) {
        items_html += '<li class="ui-state-default" id="view-entry-fields_' + fields_displayed[index] + '">' + fields_description[fields_displayed[index]] + '</li>';
    }
    $ui_displayed.append(items_html);

    // create hidden controls    
    var fields_hidden = getArrayDifference(getArrayKeys(fields_description), fields_displayed);
    
    var $ui_hidden = $('#fields_hidden');
    items_html = '';
    for (index in fields_hidden) {
        items_html += '<li class="ui-state-default" id="view-entry-fields_' + fields_hidden[index] + '">' + fields_description[ fields_hidden[index] ] + '</li>';
    }
    $ui_hidden.append(items_html);
}

function removeElement(array, element)
{
    var index = jQuery.inArray(element, array); // array.indexOf(element);
    if (index != -1) {
        array.splice(index, 1);   
    }    
}

function getArrayKeys(assoc_array)
{
    var keys = [];
    for (key in assoc_array) {
        keys.push(key);
    }
    return keys;
}

function getArrayValues(assoc_array)
{
    var values = [];
    for (key in assoc_array) {
        values.push(assoc_array[key]);
    }
    return values;
}