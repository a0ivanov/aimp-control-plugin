/*
    Script contains localization utilities.
    Copyright (c) 2014, Alexey Ivanov
*/

// global assoc array to store all localized strings.
var lang = {};
var l10n_data_files_list = ['aimp_manager', 'main_page'];

/* Gets language ID from cookie, or return default value. */
function getLanguageID() {
    var lang_id = $.cookie('language');
    return lang_id === null ? 'ru' // by default set Russian language.
                            : lang_id;
}

/* Returns relative path to l10n files on server. */
function getLocalizationDirectory() {
    return 'i18n/' + getLanguageID();
}

/* Load l10n files from l10n directory. */
function load_l10n_data() {
    var l10n_directory = getLocalizationDirectory();

    for ( file in l10n_data_files_list ) {
        document.write('<script type="text/javascript" src="' + l10n_directory + '/' + l10n_data_files_list[file] + '.js"><\/script>');
    }
}

// initialization of global string array 'lang' from separate modules l10n arrays.
function initLocalization() {

    function add_data_to_global_string_array_from(from) {
        for (key in from) {
            if (lang.hasOwnProperty(key)) {
                throw new Error('Duplicate l10n string ID: ' + key);
            }
            lang[key] = from[key];
        }
    }


    for ( file in l10n_data_files_list ) {
        // get global array of l10n data from current file.
        var l10n_array = window[ 'language_' + l10n_data_files_list[file] ];
        if (l10n_array === undefined) {
            alert('Not all localization data was loaded. Language data file "' + getLocalizationDirectory() + '/' + l10n_data_files_list[file] + '.js" is missing.');
        } else {
            // add data from current l10n file.
            add_data_to_global_string_array_from(l10n_array);
        }
    }

}

/* get localizated text by ID. */
function getText(string_id) {
    if ( lang.hasOwnProperty(string_id) ) {
        return lang[string_id];
    }
    return '';
}