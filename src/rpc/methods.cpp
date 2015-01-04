// Copyright (c) 2014, Alexey Ivanov

#include "stdafx.h"
#include "methods.h"
#include "aimp/manager.h"
#include "aimp/manager_impl_common.h"
#include "plugin/logger.h"
#include "plugin/control_plugin.h"
#include "plugin/settings.h"
#include "rpc/exception.h"
#include "rpc/value.h"
#include "rpc/request_handler.h"
#include "utils/util.h"
#include "utils/scope_guard.h"
#include "utils/string_encoding.h"
#include "utils/image.h"
#include "utils/power_management.h"
#include <fstream>
#include <boost/range.hpp>
#include <boost/bind.hpp>
#include <boost/assign/std.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/std/vector.hpp>
#include <string.h>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Rpc::RequestHandler>(); }
}

namespace AimpRpcMethods
{

const PlaylistID kPlaylistIdNotUsed = 0;

using namespace Rpc;

TrackDescription AIMPRPCMethod::getTrackDesc(const Rpc::Value& params) const // throws Rpc::Exception
{
    const bool require_playlist_id = dynamic_cast<AIMPManager26*>(&aimp_manager_) != nullptr;
    const PlaylistID playlist_id = require_playlist_id ? params["playlist_id"]
                                                       : params.isMember("playlist_id") ? params["playlist_id"]
                                                                                        : kPlaylistIdNotUsed;
    return TrackDescription(playlist_id, params["track_id"]);
}

Rpc::Value::Object emptyResult()
{
    return Rpc::Value::Object();
}

ResponseType Play::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() > 0) {
        const TrackDescription track_desc(getTrackDesc(params));
        try {
            aimp_manager_.startPlayback(track_desc);
        } catch (std::runtime_error&) {
            throw Rpc::Exception("Playback specified track failed. Track does not exist or track's source is not available.", PLAYBACK_FAILED);
        }
    } else {
        try {
            aimp_manager_.startPlayback();
        } catch (std::runtime_error&) {
            throw Rpc::Exception("Playback default failed. Reason: internal AIMP error.", PLAYBACK_FAILED);
        }
    }

    Rpc::Value& result = root_response["result"];
    RpcResultUtils::setCurrentPlaybackState(aimp_manager_.getPlaybackState(), result);
    RpcResultUtils::setCurrentPlayingSourceInfo(aimp_manager_, result);
    return RESPONSE_IMMEDIATE;
}

ResponseType Pause::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    aimp_manager_.pausePlayback();
    RpcResultUtils::setCurrentPlaybackState(aimp_manager_.getPlaybackState(), root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType Stop::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    aimp_manager_.stopPlayback();
    RpcResultUtils::setCurrentPlaybackState(aimp_manager_.getPlaybackState(), root_response["result"]); // return current playback state.
    return RESPONSE_IMMEDIATE;
}

ResponseType PlayPrevious::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    aimp_manager_.playPreviousTrack();
    RpcResultUtils::setCurrentPlayingSourceInfo(aimp_manager_, root_response["result"]); // return current playlist and track.
    return RESPONSE_IMMEDIATE;
}

ResponseType PlayNext::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    aimp_manager_.playNextTrack();
    RpcResultUtils::setCurrentPlayingSourceInfo(aimp_manager_, root_response["result"]); // return current playlist and track.
    return RESPONSE_IMMEDIATE;
}

bool Status::statusGetSetSupported(AIMPManager::STATUS status) const
{
    switch (status) {
    // do not allow to get GUI handles over network.
    case AIMPManager::STATUS_MAIN_HWND:
    case AIMPManager::STATUS_TC_HWND:
    case AIMPManager::STATUS_APP_HWND:
    case AIMPManager::STATUS_PL_HWND:
    case AIMPManager::STATUS_EQ_HWND:
    case AIMPManager::STATUS_TRAY:
        return false;
    default:
        return true;
    }
}

ResponseType Status::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    const AIMPManager::STATUS status = static_cast<AIMPManager::STATUS>(static_cast<int>(params["status_id"]));
    if ( aimpStatusValid(status) || !statusGetSetSupported(status) ) {
        if ( params.isMember("value") ) { // set mode if argument was passed.
            try {
                aimp_manager_.setStatus(status, params["value"]);
            } catch (std::runtime_error&) {
                throw Rpc::Exception("Status set failed", STATUS_SET_FAILED);
            }
        }

        // return current status value.
        root_response["result"]["value"] = aimp_manager_.getStatus(status);
        return RESPONSE_IMMEDIATE;
    } else {
        throw Rpc::Exception("Unknown status ID", WRONG_ARGUMENT);
    }
}

ResponseType ShufflePlaybackMode::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 1) { // set mode if argument was passed.
        const bool shuffle_mode_on = params["shuffle_on"];
        try {
            aimp_manager_.setStatus(AIMPManager::STATUS_SHUFFLE, shuffle_mode_on);
        } catch (std::runtime_error& e) {
            std::stringstream msg;
            msg << "Error occured while set shuffle playback mode. Reason:" << e.what();
            throw Rpc::Exception(msg.str(), SHUFFLE_MODE_SET_FAILED);
        }
    }

    // return current mode.
    RpcResultUtils::setCurrentShuffleMode(aimp_manager_.getStatus(AIMPManager::STATUS_SHUFFLE) != 0, root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType RepeatPlaybackMode::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 1) { // set mode if argument was passed.
        const bool repeat_mode_on = params["repeat_on"];
        try {
            aimp_manager_.setStatus(AIMPManager::STATUS_REPEAT, repeat_mode_on);
        } catch (std::runtime_error& e) {
            std::stringstream msg;
            msg << "Error occured while set repeat playlist playback mode. Reason:" << e.what();
            throw Rpc::Exception(msg.str(), REPEAT_MODE_SET_FAILED);
        }
    }

    // return current mode.
    RpcResultUtils::setCurrentRepeatMode(aimp_manager_.getStatus(AIMPManager::STATUS_REPEAT) != 0, root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType VolumeLevel::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 1) { // set volume if value was passed.
        const int volume_level(params["level"]);
        if ( !(0 <= volume_level && volume_level <= 100) ) {
            throw Rpc::Exception("Volume level is out of range [0, 100].", VOLUME_OUT_OF_RANGE);
        }

        try {
            aimp_manager_.setStatus(AIMPManager::STATUS_VOLUME, volume_level);
        } catch (std::runtime_error& e) {
            std::stringstream msg;
            msg << "Error occured while setting volume level. Reason:" << e.what();
            throw Rpc::Exception(msg.str(), VOLUME_SET_FAILED);
        }
    }

    // return current mode.
    RpcResultUtils::setCurrentVolume(aimp_manager_.getStatus(AIMPManager::STATUS_VOLUME), root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType Mute::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 1) { // set mode if argument was passed.
        const bool mute_on = params["mute_on"];
        try {
            aimp_manager_.setStatus(AIMPManager::STATUS_MUTE, mute_on);
        } catch (std::runtime_error& e) {
            std::stringstream msg;
            msg << "Error occured while set mute mode. Reason:" << e.what();
            throw Rpc::Exception(msg.str(), MUTE_SET_FAILED);
        }
    }

    // return current mode.
    RpcResultUtils::setCurrentMuteMode(aimp_manager_.getStatus(AIMPManager::STATUS_MUTE) != 0, root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType RadioCaptureMode::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 1) { // set mode if argument was passed.
        const bool value = params["radio_capture_on"];
        try {
            aimp_manager_.setStatus(AIMPManager::STATUS_RADIO_CAPTURE, value);
        } catch (std::runtime_error& e) {
            std::stringstream msg;
            msg << "Error occured while set radio capture mode. Reason:" << e.what();
            throw Rpc::Exception(msg.str(), RADIO_CAPTURE_MODE_SET_FAILED);
        }
    }

    // return current mode.
    RpcResultUtils::setCurrentRadioCaptureMode(aimp_manager_.getStatus(AIMPManager::STATUS_RADIO_CAPTURE) != 0, root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType GetFormattedEntryTitle::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];

    const TrackDescription track_desc(getTrackDesc(params));
    try {
        using namespace StringEncoding;
        root_response["result"]["formatted_string"] = utf16_to_utf8( aimp_manager_.getFormattedEntryTitle(track_desc, params["format_string"]) );
    } catch(std::runtime_error&) {
        throw Rpc::Exception("Specified track does not exist.", TRACK_NOT_FOUND);
    } catch (std::invalid_argument&) {
        throw Rpc::Exception("Wrong specified format string", WRONG_ARGUMENT);
    }

    return RESPONSE_IMMEDIATE;
}

ResponseType EnqueueTrack::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    const TrackDescription track_desc(getTrackDesc(params));
    const bool insert_at_queue_beginning = params.isMember("insert_at_queue_beginning") ? bool(params["insert_at_queue_beginning"]) 
                                                                                        : false; // by default insert at the end of queue.

    try {
        aimp_manager_.enqueueEntryForPlay(track_desc, insert_at_queue_beginning);
        root_response["result"] = emptyResult();
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Enqueue track failed. Reason: internal AIMP error.", ENQUEUE_TRACK_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

ResponseType QueueTrackMove::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    if ( AIMPPlayer::IPlaylistQueueManager* playlist_queue_manager = dynamic_cast<AIMPPlayer::IPlaylistQueueManager*>(&aimp_manager_) ) {
        const Rpc::Value& params = root_request["params"];

        try {
            if (params.isMember("old_queue_index")) {
                playlist_queue_manager->moveQueueEntry(params["old_queue_index"], params["new_queue_index"]);
            } else {
                const TrackDescription track_desc(getTrackDesc(params));
                playlist_queue_manager->moveQueueEntry(track_desc, params["new_queue_index"]);
            }            
            root_response["result"] = emptyResult();
        } catch (std::runtime_error&) {
            throw Rpc::Exception("Enqueue track failed. Reason: internal AIMP error.", MOVE_TRACK_IN_QUEUE_FAILED);
        }
        return RESPONSE_IMMEDIATE;
    }
    throw Rpc::Exception("Not supported by this version of AIMP", METHOD_NOT_FOUND_ERROR);
}

ResponseType RemoveTrackFromPlayQueue::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    const TrackDescription track_desc(getTrackDesc(params));
    try {
        aimp_manager_.removeEntryFromPlayQueue(track_desc);
        root_response["result"] = emptyResult();
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Removing track from play queue failed. Reason: internal AIMP error.", DEQUEUE_TRACK_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

std::string GetPlaylists::getColumnsString() const
{
    std::string result;
    const auto& setters = playlist_fields_filler_.setters_required_;

    BOOST_FOREACH(auto& setter_it, setters) {
        result += setter_it->first;
        result += ',';
    }

    // erase obsolete ',' character.
    if ( !result.empty() ) {
        result.pop_back();
    }
    return result;
}

ResponseType GetPlaylists::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    using namespace Utilities;

    const Rpc::Value& params = root_request["params"];

    // get list of required pairs(field id, field getter function).    
    if ( params.isMember("fields") ) {
        playlist_fields_filler_.initRequiredFieldsHandlersList(params["fields"]);
    } else {
        // set default fields
        Rpc::Value fields;
        fields.setSize(2);
        fields[0] = "id";
        fields[1] = "title";
        playlist_fields_filler_.initRequiredFieldsHandlersList(fields);
    }

    std::ostringstream query;
    query << "SELECT " << getColumnsString() << " FROM Playlists ORDER BY playlist_index";

    sqlite3* playlists_db = AIMPPlayer::getPlaylistsDB(aimp_manager_);
    sqlite3_stmt* stmt = createStmt( playlists_db,
                                     query.str().c_str()
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    assert( static_cast<size_t>( sqlite3_column_count(stmt) ) == playlist_fields_filler_.setters_required_.size() );

    Rpc::Value& playlists_rpcvalue = root_response["result"];

    size_t playlist_rpcvalue_index = 0;
    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            playlists_rpcvalue.setSize(playlist_rpcvalue_index + 1); /// TODO: if possible resize array full count of found rows before filling.
            Rpc::Value& playlist_rpcvalue = playlists_rpcvalue[playlist_rpcvalue_index];
            // fill all requested fields for playlist.
            playlist_fields_filler_.fillRpcArrayOfObjects(stmt, playlist_rpcvalue);
            ++playlist_rpcvalue_index;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    return RESPONSE_IMMEDIATE;
}

struct Formatter {
    const AIMPManager* aimp_manager_;
    const std::string* format_string_;
    Formatter(const AIMPManager* aimp_manager, const std::string* format_string)
        :
        aimp_manager_(aimp_manager),
        format_string_(format_string)
    {}

    void operator()(sqlite3_stmt* stmt, int /*column_index*/, Rpc::Value& rpc_value) const {
        /*
            Here we use small hack: 
                we must support signature of RpcValueSetHelpers::HelperFillRpcFields::RpcValueSetter,
                but we need to know more than 1 field: playlist and entry ids.
            So we treat that always have two fields in db row when format string is set in RPC request.
        */
        const int playlist_column_index = 0,
                  track_column_index    = 1;
        TrackDescription track_desc(sqlite3_column_int(stmt, playlist_column_index),
                                    sqlite3_column_int(stmt, track_column_index)
                                    );
        
        rpc_value = StringEncoding::utf16_to_utf8( aimp_manager_->getFormattedEntryTitle(track_desc, 
                                                                                         *format_string_)
                                                  );
    }
};

GetPlaylistEntries::GetPlaylistEntries(AIMPManager& aimp_manager,
                                       Rpc::RequestHandler& rpc_request_handler
                                       )
    :
    AIMPRPCMethod("GetPlaylistEntries", aimp_manager, rpc_request_handler),
    entry_fields_filler_("entry"),
    kRQST_KEY_FORMAT_STRING("format_string"),
    kRQST_KEY_FIELDS("fields"),
    kRQST_KEY_START_INDEX("start_index"),
    kRQST_KEY_ENTRIES_COUNT("entries_count"),
    kRQST_KEY_FIELD("field"),
    kDESCENDING_ORDER_STRING("desc"),
    kRQST_KEY_ORDER_DIRECTION("dir"),
    kRQST_KEY_ORDER_FIELDS("order_fields"),
    kRQST_KEY_SEARCH_STRING("search_string"),
    kRSLT_KEY_TOTAL_ENTRIES_COUNT("total_entries_count"),
    kRSLT_KEY_ENTRIES("entries"),
    kRSLT_KEY_COUNT_OF_FOUND_ENTRIES("count_of_found_entries"),
    kRQST_KEY_FIELD_PLAYLIST_ID("playlist_id"),
    kRSLT_KEY_FIELD_QUEUE_INDEX("queue_index"),
    kRQST_KEY_FIELD_FOLDER_NAME("foldername"),
    pagination_info_(nullptr),
    queued_entries_mode_(false)
{
    using namespace RpcValueSetHelpers;
    using namespace RpcResultUtils;

    auto int_setter   = boost::bind( createSetter(&sqlite3_column_int),   _1, _2, _3 );
    auto int64_setter = boost::bind( createSetter(&sqlite3_column_int64), _1, _2, _3 );
    auto text_setter  = boost::bind( createSetter(&sqlite3_column_text),  _1, _2, _3 );
    auto folder_name_setter = [](sqlite3_stmt* stmt, int column_index, Rpc::Value& rpc_value) {
        const unsigned char* path = sqlite3_column_text(stmt, column_index);
        rpc_value = getFolderNameFromPath(reinterpret_cast<const char*>(path));
    };

    boost::assign::insert(entry_fields_filler_.setters_)
        ( getStringFieldID(PlaylistEntry::ID),       int_setter )
        ( getStringFieldID(PlaylistEntry::TITLE),    text_setter )
        ( getStringFieldID(PlaylistEntry::ARTIST),   text_setter )
        ( getStringFieldID(PlaylistEntry::ALBUM),    text_setter )
        ( getStringFieldID(PlaylistEntry::DATE),     text_setter )
        ( getStringFieldID(PlaylistEntry::GENRE),    text_setter )
        ( getStringFieldID(PlaylistEntry::BITRATE),  int_setter )
        ( getStringFieldID(PlaylistEntry::DURATION), int_setter )
        ( getStringFieldID(PlaylistEntry::FILESIZE), int64_setter )
        ( getStringFieldID(PlaylistEntry::RATING),   int_setter )
        ( kRQST_KEY_FIELD_FOLDER_NAME,               folder_name_setter) // For now only folder name is supported.
    ;

    // map RPC field names to related db field names. All field names are the same except rpc's 'id' is 'entry_id' in db.
    BOOST_FOREACH(auto& setter_it, entry_fields_filler_.setters_) {
        fieldnames_rpc_to_db_[setter_it.first] = setter_it.first;
    }
    fieldnames_rpc_to_db_[getStringFieldID(PlaylistEntry::ID)] = "entry_id";
    fieldnames_rpc_to_db_[kRQST_KEY_FIELD_FOLDER_NAME] = getStringFieldID(PlaylistEntry::FILENAME);

    // fields available for order.
    using namespace boost::assign;
    fields_to_order_ += 
        getStringFieldID(PlaylistEntry::ID),
        getStringFieldID(PlaylistEntry::TITLE),
        getStringFieldID(PlaylistEntry::ARTIST),
        getStringFieldID(PlaylistEntry::ALBUM),
        getStringFieldID(PlaylistEntry::DATE),
        getStringFieldID(PlaylistEntry::GENRE),
        getStringFieldID(PlaylistEntry::BITRATE),
        getStringFieldID(PlaylistEntry::DURATION),
        getStringFieldID(PlaylistEntry::FILESIZE),
        getStringFieldID(PlaylistEntry::RATING)
    ;

    // fields available to filtering.
    fields_to_filter_ +=
        getStringFieldID(PlaylistEntry::TITLE),
        getStringFieldID(PlaylistEntry::ARTIST),
        getStringFieldID(PlaylistEntry::ALBUM),
        getStringFieldID(PlaylistEntry::DATE),
        getStringFieldID(PlaylistEntry::GENRE)
    ;
}

void GetPlaylistEntries::addSpecialFieldsSupport()
{
    using namespace RpcValueSetHelpers;
    auto int_setter   = boost::bind( createSetter(&sqlite3_column_int),   _1, _2, _3 );
    // playlist_id field
    entry_fields_filler_.setters_[kRQST_KEY_FIELD_PLAYLIST_ID] = int_setter;
    fieldnames_rpc_to_db_        [kRQST_KEY_FIELD_PLAYLIST_ID] = kRQST_KEY_FIELD_PLAYLIST_ID;
    // queue_index field
    entry_fields_filler_.setters_[kRSLT_KEY_FIELD_QUEUE_INDEX] = int_setter;
    fieldnames_rpc_to_db_        [kRSLT_KEY_FIELD_QUEUE_INDEX] = kRSLT_KEY_FIELD_QUEUE_INDEX;
}

void GetPlaylistEntries::removeSpecialFieldsSupport()
{
    // playlist_id field
    entry_fields_filler_.setters_.erase(kRQST_KEY_FIELD_PLAYLIST_ID);
    fieldnames_rpc_to_db_.        erase(kRQST_KEY_FIELD_PLAYLIST_ID);
    // queue_index field
    entry_fields_filler_.setters_.erase(kRSLT_KEY_FIELD_QUEUE_INDEX);
    fieldnames_rpc_to_db_.        erase(kRSLT_KEY_FIELD_QUEUE_INDEX);
}

void GetPlaylistEntries::initEntriesFiller(const Rpc::Value& params)
{
    if ( params.isMember(kRQST_KEY_FORMAT_STRING) ) { // 'format_string' param has priority over 'fields' param.
        typedef RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry>::RpcValueSetters RpcValueSetters;
        RpcValueSetters::iterator setter_it = entry_fields_filler_.setters_.find(kRQST_KEY_FORMAT_STRING);
        if ( setter_it == entry_fields_filler_.setters_.end() ) {
            // add filler if it does not exist yet.
            setter_it = entry_fields_filler_.setters_.insert( std::make_pair(kRQST_KEY_FORMAT_STRING,
                                                                             RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry>::RpcValueSetter()
                                                                             )
                                                             ).first;
        }
        
        const std::string& format_string = params[kRQST_KEY_FORMAT_STRING];
        setter_it->second = boost::bind<void>(Formatter(&aimp_manager_, &format_string),
                                              _1, _2, _3
                                              );
        entry_fields_filler_.setters_required_.clear();
        entry_fields_filler_.setters_required_.push_back(setter_it);
    } else if ( params.isMember(kRQST_KEY_FIELDS) ) {
        entry_fields_filler_.initRequiredFieldsHandlersList(params[kRQST_KEY_FIELDS]);
    } else {
        // set default fields
        Rpc::Value fields;
        fields.setSize(2);
        fields[0] = "id";
        fields[1] = "title";
        entry_fields_filler_.initRequiredFieldsHandlersList(fields);
    }
}

std::string GetPlaylistEntries::getOrderString(const Rpc::Value& params) const
{
    std::string result;
    if (!queuedEntriesMode()) {
	    if ( params.isMember(kRQST_KEY_ORDER_FIELDS) ) {        
            const Rpc::Value& entry_fields_to_order = params[kRQST_KEY_ORDER_FIELDS];
            const size_t fields_count = entry_fields_to_order.size();
            for (size_t field_index = 0; field_index < fields_count; ++field_index) {
                const Rpc::Value& field_desc = entry_fields_to_order[field_index];
                const std::string& field_to_order = field_desc[kRQST_KEY_FIELD];
                const auto supported_field_it = std::find(fields_to_order_.begin(), fields_to_order_.end(), field_to_order);
                if ( supported_field_it != fields_to_order_.end() ) {
                    if ( result.empty() ) {
                        result = "ORDER BY ";
                    }
                    result += field_to_order;
                    result += (field_desc[kRQST_KEY_ORDER_DIRECTION] == kDESCENDING_ORDER_STRING) ? " DESC," 
                                                                                                  : " ASC,";
                }
            }

            if (!result.empty() && result.back() == ',') {
                result.pop_back();
            }
        }

        // by default order by index to have AIMP playlist's order.
        if (result.empty()) {
            result = "ORDER BY entry_index ASC";
        }
    } else {
        result = "ORDER BY queue_index ASC";
    }
    return result;
}

std::string GetPlaylistEntries::getLimitString(const Rpc::Value& params) const
{
    std::ostringstream os;
	if ( params.isMember(kRQST_KEY_START_INDEX) && params.isMember(kRQST_KEY_ENTRIES_COUNT) ) {
        const int entries_count = params[kRQST_KEY_ENTRIES_COUNT];
        if (entries_count != -1) { // -1 is special value which means "all available items". Included to support jQuery Datatables 1.7.6.
            const int start_entry_index = params[kRQST_KEY_START_INDEX];
	        os << "LIMIT " << start_entry_index << ',' << entries_count;
        }

        if ( entryLocationDeterminationMode() ) {
            pagination_info_->entries_on_page_ = entries_count;
        }
    }
    return os.str();
}

std::string GetPlaylistEntries::getWhereString(const Rpc::Value& params, const int playlist_id) const
{
    using namespace Utilities;

    struct LikeArgSetter : public std::binary_function<sqlite3_stmt*, int, void>
    {
        typedef std::string StringT;
        StringT like_arg_;
        LikeArgSetter(const StringT& like_arg) : like_arg_(like_arg) {}
        void operator()(sqlite3_stmt* stmt, int bind_index) const {
            const int rc_db = sqlite3_bind_text(stmt, bind_index,
                                                like_arg_.c_str(),
                                                like_arg_.size() * sizeof(StringT::value_type),
                                                SQLITE_TRANSIENT);
            if (SQLITE_OK != rc_db) {
                const std::string msg = MakeString() << "Error sqlite3_bind_text: " << rc_db;
                throw std::runtime_error(msg);
            }
        }
    };

    std::ostringstream os;
    
    if (!queuedEntriesMode()) {
        os << "WHERE playlist_id=" << playlist_id;
    }

	if ( params.isMember(kRQST_KEY_SEARCH_STRING) ) {
        const std::string& search_string = params[kRQST_KEY_SEARCH_STRING];
        if ( !search_string.empty() && !fields_to_filter_.empty() ) { ///??? search in all fields or only in requested ones.
            const std::string like_arg = '%' + search_string + '%';
            const QueryArgSetter& setter = boost::bind<void>(LikeArgSetter(like_arg), _1, _2);
            
            if (!queuedEntriesMode()) {
                os << " AND (";
            } else {
                os << " WHERE (";
            }
            FieldNames::const_iterator begin = fields_to_filter_.begin(),
                                       end   = fields_to_filter_.end();
            for (FieldNames::const_iterator fieldname_it = begin;
                                            fieldname_it != end;
                                            ++fieldname_it
                 )
            {
                query_arg_setters_.push_back(setter);

                os << *fieldname_it << " LIKE ?";
                if (fieldname_it + 1 != end) {
                    os << " OR ";
                }
            }
            os << ")";
        }
    }
    return os.str();
}

std::string GetPlaylistEntries::getColumnsString() const
{
    std::string result;
    const auto& setters = entry_fields_filler_.setters_required_;
    
    // special case: format string. It is not database field and for it's work we must use playlist and entry ids.
    if ( !setters.empty() ) {
        if (setters.front()->first == kRQST_KEY_FORMAT_STRING) {
            return "playlist_id,entry_id";
        }
    }

    BOOST_FOREACH(auto& setter_it, setters) {
        result += fieldnames_rpc_to_db_.find(setter_it->first)->second; // fieldnames_rpc_to_db_ is syncronized with setters.
        result += ',';
    }

    // erase obsolete ',' character.
    if ( !result.empty() ) {
        result.pop_back();
    }
    return result;
}

size_t GetPlaylistEntries::getTotalEntriesCount(sqlite3* playlists_db, const int playlist_id) const
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT COUNT(*) FROM ";
    if (!queuedEntriesMode()) {
        query << "PlaylistsEntries WHERE playlist_id=" << playlist_id;
    } else {
        query << "QueuedEntries";
    }

    sqlite3_stmt* stmt = createStmt( playlists_db,
                                     query.str()
                                    );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    size_t total_entries_count = 0;

	const int rc_db = sqlite3_step(stmt);
    if (SQLITE_ROW == rc_db) {
        assert(sqlite3_column_count(stmt) > 0);
        total_entries_count = sqlite3_column_int(stmt, 0);
    } else {
        const std::string msg = MakeString() << "sqlite3_step() error "
                                             << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                             << ". Query: " << query.str();
        throw std::runtime_error(msg);
    }

    return total_entries_count;
}

Rpc::ResponseType GetPlaylistEntries::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    using namespace Utilities;

    PROFILE_EXECUTION_TIME(__FUNCTION__);
 
    ON_BLOCK_EXIT_OBJ(*this, &GetPlaylistEntries::deactivateEntryLocationDeterminationMode);
    ON_BLOCK_EXIT_OBJ(*this, &GetPlaylistEntries::deactivateQueuedEntriesMode);
    ON_BLOCK_EXIT_OBJ(*this, &GetPlaylistEntries::removeSpecialFieldsSupport);

    const Rpc::Value& params = root_request["params"];

    if (queuedEntriesMode()) {
        addSpecialFieldsSupport();
    }

    initEntriesFiller(params);

    if (!queuedEntriesMode()) {
        // ensure we got obligatory argument: playlist id.
        if (params.size() < 1) {
            throw Rpc::Exception("Wrong arguments count. Wait at least int 'playlist_id' argument.", WRONG_ARGUMENT);
        }
    }

    const int playlist_id_not_used = 0;
    const int playlist_id = !queuedEntriesMode() ? aimp_manager_.getAbsolutePlaylistID(params["playlist_id"])
                                                 : playlist_id_not_used;

    query_arg_setters_.clear();

    std::ostringstream query_without_limit,
                       query_with_limit;
    query_without_limit << "SELECT " << getColumnsString() << " FROM "
                        << (!queuedEntriesMode() ? "PlaylistsEntries" : "QueuedEntries")
                        << ' '   
                        << getWhereString(params, playlist_id) << ' ' 
                        << getOrderString(params);
    query_with_limit << query_without_limit.str() << ' '
                     << getLimitString(params);

    const std::string query = !entryLocationDeterminationMode() ? query_with_limit.str() : query_without_limit.str();

    sqlite3* playlists_db = AIMPPlayer::getPlaylistsDB(aimp_manager_);
    sqlite3_stmt* stmt = createStmt( playlists_db, query.c_str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    // bind all query args.
    size_t bind_index = 1;
    BOOST_FOREACH(auto& setter, query_arg_setters_) {
        setter(stmt, bind_index++);
    }

#ifdef _DEBUG
    const auto& setters = entry_fields_filler_.setters_required_;
    if ( !(    !setters.empty()
            && setters.front()->first == kRQST_KEY_FORMAT_STRING
           ) 
        )
    {
        assert( static_cast<size_t>( sqlite3_column_count(stmt) ) == setters.size() );
    }
#endif

    if ( !entryLocationDeterminationMode() ) {
        Rpc::Value& rpc_result = root_response["result"];
        Rpc::Value& rpcvalue_entries  = rpc_result[kRSLT_KEY_ENTRIES];
        rpcvalue_entries.setSize(0); // return zero-length array, not null if no entires found.

        size_t entry_index = 0;
        for(;;) {
		    int rc_db = sqlite3_step(stmt);
            if (SQLITE_ROW == rc_db) {
                rpcvalue_entries.setSize(entry_index + 1); /// TODO: if possible resize array full count of found rows before filling.
                Rpc::Value& entry_rpcvalue = rpcvalue_entries[entry_index];
                // fill all requested fields for entry.
                entry_fields_filler_.fillRpcArrayOfArrays(stmt, entry_rpcvalue);
                ++entry_index;
            } else if (SQLITE_DONE == rc_db) {
                break;
            } else {
                const std::string msg = MakeString() << "sqlite3_step() error "
                                                     << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                                     << ". Query: " << query;
                throw std::runtime_error(msg);
		    }
        }

        rpc_result[kRSLT_KEY_TOTAL_ENTRIES_COUNT]    = getTotalEntriesCount(playlists_db, playlist_id);
        rpc_result[kRSLT_KEY_COUNT_OF_FOUND_ENTRIES] = getRowsCount(playlists_db, query_without_limit.str(), &query_arg_setters_);
    } else {
        size_t entry_index = 0;
        for(;;) {
		    int rc_db = sqlite3_step(stmt);
            if (SQLITE_ROW == rc_db) {
                const int entry_id = sqlite3_column_int(stmt, pagination_info_->id_field_index);
                if (pagination_info_->entry_id == entry_id) {
                    pagination_info_->entry_index_in_current_representation_ = entry_index;
                    break;
                }                
                ++entry_index;
            } else if (SQLITE_DONE == rc_db) {
                break;
            } else {
                const std::string msg = MakeString() << "sqlite3_step() error "
                                                     << rc_db << ": " << sqlite3_errmsg(playlists_db)
                                                     << ". Query: " << query;
                throw std::runtime_error(msg);
		    }
        }
    }

    return RESPONSE_IMMEDIATE;
}

GetEntryPositionInDataTable::GetEntryPositionInDataTable(AIMPManager& aimp_manager,
                                                         Rpc::RequestHandler& rpc_request_handler,
                                                         GetPlaylistEntries& getplaylistentries_method
                                                         )
    :
    AIMPRPCMethod("GetEntryPositionInDataTable", aimp_manager, rpc_request_handler),
    getplaylistentries_method_(getplaylistentries_method),
    kFIELD_ID("id")
{
}

Rpc::ResponseType GetEntryPositionInDataTable::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    Rpc::Value root_request_copy = root_request;

    Rpc::Value& rpc_params = root_request_copy["params"];
    const PlaylistEntryID track_id(rpc_params["track_id"]);

    int id_field_index = -1;
    Rpc::Value& fields = rpc_params["fields"];
    for (int i = 0, end = fields.size(); i != end; ++i) {
        const std::string& field = fields[i];
        if (field == kFIELD_ID) {
            id_field_index = i;
            break;
        }
    }
    if (id_field_index < 0) {
        id_field_index = fields.size();
        fields.setSize(id_field_index + 1);
        fields[id_field_index] = kFIELD_ID;
    }

    PaginationInfo pagination_info(track_id, id_field_index);
    getplaylistentries_method_.activateEntryLocationDeterminationMode(&pagination_info);
    getplaylistentries_method_.execute(root_request_copy, root_response);

    const size_t entries_on_page = pagination_info.entries_on_page_;
    const int entry_index_in_current_representation = pagination_info.entry_index_in_current_representation_;

    //const size_t entries_count_in_representation = current_filtered_entries_count_ != 0 ? current_filtered_entries_count_ : current_total_entries_count_;
    Rpc::Value& rpc_result = root_response["result"];
    if (entries_on_page > 0 && entry_index_in_current_representation >= 0) {
        rpc_result["page_number"]         = static_cast<size_t>(entry_index_in_current_representation / entries_on_page);
        rpc_result["track_index_on_page"] = static_cast<size_t>(entry_index_in_current_representation % entries_on_page);
    } else {
        rpc_result["page_number"] = -1; ///??? maybe return null here or nothing.
        rpc_result["track_index_on_page"] = -1;
    }
    return RESPONSE_IMMEDIATE;
}

GetQueuedEntries::GetQueuedEntries(AIMPManager& aimp_manager,
                                   Rpc::RequestHandler& rpc_request_handler,
                                   GetPlaylistEntries& getplaylistentries_method
                                   )
    :
    AIMPRPCMethod("GetQueuedEntries", aimp_manager, rpc_request_handler),
    getplaylistentries_method_(getplaylistentries_method)
{
}

Rpc::ResponseType GetQueuedEntries::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    if ( AIMPPlayer::IPlaylistQueueManager* playlist_queue_manager = dynamic_cast<AIMPPlayer::IPlaylistQueueManager*>(&aimp_manager_) ) {
        playlist_queue_manager->reloadQueuedEntries();
        getplaylistentries_method_.activateQueuedEntriesMode();
        return getplaylistentries_method_.execute(root_request, root_response);
    }
    throw Rpc::Exception("Not supported by this version of AIMP", METHOD_NOT_FOUND_ERROR);
}

ResponseType GetPlaylistEntriesCount::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() != 1) {
        throw Rpc::Exception("Wrong arguments count. Wait one integer value: playlist_id.", WRONG_ARGUMENT);
    }

    const size_t entries_count = AIMPPlayer::getEntriesCountDB(params["playlist_id"], AIMPPlayer::getPlaylistsDB(aimp_manager_));
    root_response["result"] = static_cast<int>(entries_count); // max int value overflow is possible, but I doubt that we will work with such huge playlists.
    return RESPONSE_IMMEDIATE;
}

std::string text16_to_utf8(const void* text16) 
{
    const WCHAR* text = static_cast<const WCHAR*>(text16);
    return StringEncoding::utf16_to_utf8( text, text + wcslen(text) );
}

ResponseType GetPlaylistEntryInfo::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];

    const TrackDescription track_desc = aimp_manager_.getAbsoluteTrackDesc( getTrackDesc(params) );

    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT "
          << "playlist_id, entry_id, album, artist, date, genre, title, bitrate, channels_count, duration, filesize, rating, samplerate"
          << " FROM PlaylistsEntries WHERE entry_id=" << track_desc.track_id;
    if (track_desc.playlist_id != kPlaylistIdNotUsed) {
        query << " AND playlist_id=" << track_desc.playlist_id;
    }

    sqlite3* db = getPlaylistsDB(aimp_manager_);
    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            assert(sqlite3_column_count(stmt) == 13);

            using namespace RpcResultUtils;
            Value& result = root_response["result"];
            result["playlist_id"                            ] =                             sqlite3_column_int   (stmt, 0);
            result[getStringFieldID(PlaylistEntry::ID)      ] =                             sqlite3_column_int   (stmt, 1);
            result[getStringFieldID(PlaylistEntry::ALBUM)   ] =             text16_to_utf8( sqlite3_column_text16(stmt, 2) );
            result[getStringFieldID(PlaylistEntry::ARTIST)  ] =             text16_to_utf8( sqlite3_column_text16(stmt, 3) );
            result[getStringFieldID(PlaylistEntry::DATE)    ] =             text16_to_utf8( sqlite3_column_text16(stmt, 4) );
            result[getStringFieldID(PlaylistEntry::GENRE)   ] =             text16_to_utf8( sqlite3_column_text16(stmt, 5) );
            result[getStringFieldID(PlaylistEntry::TITLE)   ] =             text16_to_utf8( sqlite3_column_text16(stmt, 6) );
            result[getStringFieldID(PlaylistEntry::BITRATE) ] =                             sqlite3_column_int   (stmt, 7);
            result[getStringFieldID(PlaylistEntry::CHANNELS_COUNT)] =                       sqlite3_column_int   (stmt, 8);
            result[getStringFieldID(PlaylistEntry::DURATION)] =                             sqlite3_column_int   (stmt, 9);
            result[getStringFieldID(PlaylistEntry::FILESIZE)] = static_cast<unsigned int>(  sqlite3_column_int64 (stmt,10) );
            result[getStringFieldID(PlaylistEntry::RATING)  ] =                             sqlite3_column_int   (stmt,11);
            result[getStringFieldID(PlaylistEntry::SAMPLE_RATE)] =                          sqlite3_column_int   (stmt,12);
            return RESPONSE_IMMEDIATE;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    throw Rpc::Exception("Getting info about track failed. Reason: track not found.", TRACK_NOT_FOUND);
}

const wchar_t* getExtensionByFormatId(int format_id)
{
    using namespace AIMP36SDK;
    switch (format_id) {
    case AIMP_IMAGE_FORMAT_BMP: return L".bmp";
    case AIMP_IMAGE_FORMAT_GIF: return L".gif";
    case AIMP_IMAGE_FORMAT_JPG: return L".jpg";
    case AIMP_IMAGE_FORMAT_PNG: return L".png";
    case AIMP_IMAGE_FORMAT_UNKNOWN: return L"";
    }

    assert(!"unsupported format id");
    return L".unsupported";
}

ResponseType GetCover::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];

    const TrackDescription track_desc( aimp_manager_.getAbsoluteTrackDesc( getTrackDesc(params) ) );

    int cover_width;
    int cover_height;
    try {
        cover_width = params["cover_width"];
        cover_height = params["cover_height"];

        const int kMAX_ALBUMCOVER_SIZE = 2000; // 2000x2000 is max size of cover.

        if  ( (cover_width < 0 || cover_width > kMAX_ALBUMCOVER_SIZE)
              || (cover_height < 0 || cover_height > kMAX_ALBUMCOVER_SIZE)
            )
        {
            throw Rpc::Exception(""); // limit cover size [0, kMAX_ALBUMCOVER_SIZE], use original size if any.
        }
    } catch (Rpc::Exception&) {
        cover_width = cover_height = 0; // by default request full size cover.
    }

    Cache::SearchResult cache_search_result;
    fs::wpath album_cover_filename;

    AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_);
    boost::intrusive_ptr<AIMP36SDK::IAIMPImageContainer> container;
    boost::intrusive_ptr<AIMP36SDK::IAIMPImage> image;
    boost::intrusive_ptr<AIMP36SDK::IAIMPHashCode> cover_hash;

    cache_search_result = cache_.isCoverCachedForCurrentTrack(track_desc, cover_width, cover_height);
    if (!cache_search_result) {
        if (!aimp36_manager) {
            if (aimp_manager_.isCoverImageFileExist(track_desc, &album_cover_filename)) {
                cache_search_result = cache_.isCoverCachedForAnotherTrack(album_cover_filename, cover_width, cover_height);
                if (cache_search_result) {
                    cache_.cacheBasedOnPreviousResult(track_desc, cache_search_result);    
                }
            }
        } else {
            // aimp 36: use cover hash to find out image in cache.
            if (aimp36_manager->getCoverImageContainter(track_desc, &container, &image)) {
                if (container) {
                    AIMP36SDK::IAIMPHashCode* hash_code;
                    HRESULT r = container->QueryInterface(AIMP36SDK::IID_IAIMPHashCode,
                                                          reinterpret_cast<void**>(&hash_code)
                                                          );
                    if (S_OK == r) {
                        cover_hash.reset(hash_code);
                        hash_code->Release();

                        const int cover_hash_code = cover_hash->GetHashCode();
                        cache_search_result = cache_.isCoverCachedForAnotherTrack(cover_hash_code, cover_width, cover_height);
                        if (cache_search_result) {
                            cache_.cacheBasedOnPreviousResult(track_desc, cache_search_result, &cover_hash_code);    
                        }
                    }
                }
            }
        }
    }

    if (cache_search_result) {
        // picture already exists.
        try {
            auto uri_it = cache_search_result.entry->filenames->begin();
            std::advance(uri_it, cache_search_result.uri_index);
            root_response["result"]["album_cover_uri"] = StringEncoding::utf16_to_utf8(*uri_it);
        } catch (StringEncoding::EncodingError&) {
            throw Rpc::Exception("Getting cover failed. Reason: encoding to UTF-8 failed.", ALBUM_COVER_LOAD_FAILED);
        }
        return RESPONSE_IMMEDIATE;
    }

    // save cover to temp directory with unique filename.
    boost::filesystem::wpath cover_uri (cover_directory_relative_ / getTempFileNameForAlbumCover(track_desc, cover_width, cover_height));
    cover_uri.replace_extension(L".jpg");

    boost::filesystem::wpath temp_unique_filename (document_root_ / cover_uri);

    const bool album_cover_image_file_exists = !album_cover_filename.empty();

    try {
        if (   (cover_width == 0 && cover_height == 0) // use direct copy only if no scaling is requested.
            && album_cover_image_file_exists
            )
        {
            cover_uri.replace_extension           (album_cover_filename.extension());
            temp_unique_filename.replace_extension(album_cover_filename.extension());

            fs::copy_file(album_cover_filename, temp_unique_filename);
        } else if (   (cover_width == 0 && cover_height == 0) // use direct copy only if no scaling is requested.
                   && container
                   )
        {
            // Save file via image container to original format.
            SIZE s;
            int format_id;
            HRESULT r = container->GetInfo(&s, &format_id);
            if (S_OK != r) {
                throw std::runtime_error(Utilities::MakeString() << "container->GetInfo() failed. Result: " << r);
            }

            const wchar_t* extension = getExtensionByFormatId(format_id);
            cover_uri.replace_extension           (extension);
            temp_unique_filename.replace_extension(extension);
            if (!container->GetData()) {
                throw std::runtime_error(Utilities::MakeString() << "container->GetData() is null");
            }

            std::ofstream file(temp_unique_filename.native(), std::ios_base::out | std::ios_base::binary);
            if ( file.good() ) {
                file.write(reinterpret_cast<const char*>(container->GetData()), container->GetDataSize());
                if (!file.good()) {
                    throw std::runtime_error(Utilities::MakeString() << "Failed to write to cover file : file.rdstate: " << file.rdstate());
                }
                file.close();
            } else {
                throw std::runtime_error(Utilities::MakeString() << "Failed to open file for cover writing: file.rdstate: " << file.rdstate());
            }
        } else {
            if (!free_image_dll_is_available_) {
                Rpc::Exception e("Getting cover failed. Reason: FreeImage DLLs are not available.", ALBUM_COVER_LOAD_FAILED);
                BOOST_LOG_SEV(logger(), error) << "Getting cover failed in "__FUNCTION__ << ". Reason: " << e.message();
                throw e;
            }

            aimp_manager_.saveCoverToFile(track_desc, temp_unique_filename.native(), cover_width, cover_height);
        }

        const std::wstring& cover_uri_generic = cover_uri.generic_wstring();
        root_response["result"]["album_cover_uri"] = StringEncoding::utf16_to_utf8(cover_uri_generic);

        const int cover_hash_code = cover_hash ? cover_hash->GetHashCode() : 0;
        const int* cover_hash_ptr = cover_hash ? &cover_hash_code : nullptr;
        cache_.cacheNew(track_desc, album_cover_filename, cover_uri_generic, cover_hash_ptr);
    } catch (fs::filesystem_error& e) {
        BOOST_LOG_SEV(logger(), error) << "Getting cover failed in "__FUNCTION__ << ". Reason: " << e.what();
        throw Rpc::Exception("Getting cover failed. Reason: bad temporary directory for store covers.", ALBUM_COVER_LOAD_FAILED);
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Getting cover failed in "__FUNCTION__ << ". Reason: " << e.what();
        throw Rpc::Exception("Getting cover failed. Reason: album cover extraction or saving error.", ALBUM_COVER_LOAD_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

void remove_read_only_attribute(const fs::wpath& path);

void GetCover::prepare_cover_directory() // throws runtime_error
{
    namespace fs = boost::filesystem;
    using namespace Utilities;

    try {
        // cleare cache before using.
        const fs::wpath& cover_dir = cover_directory();

        if (fs::exists(cover_dir)) {
            remove_read_only_attribute(cover_dir);
            fs::remove_all(cover_dir);
        }
    
        // create cache directory
        fs::create_directory(cover_dir);
    } catch (fs::filesystem_error& e) {
        throw std::runtime_error( MakeString() << "album cover cache directory preparation failure. Reason: " << e.what() );
    }
}

void remove_read_only_attribute(const fs::wpath& path)
{
    const auto path_native = path.native();
    if (!SetFileAttributes(path_native.c_str(), GetFileAttributes(path_native.c_str()) & ~FILE_ATTRIBUTE_READONLY)) {
        throw std::runtime_error(Utilities::MakeString() << "Fail to remove read-only attribute from file " << path_native.c_str() << ". Reason: " << GetLastError());
    }
}

void GetCover::Cache::cacheNew(TrackDescription track_desc, const fs::wpath& album_cover_filename, const std::wstring& cover_uri_generic, const int* cover_hash)
{
    Entry& entry = track_desc_entry_map_[track_desc];
    if (!album_cover_filename.empty()) {
        path_track_map_.insert( std::make_pair(album_cover_filename, track_desc) );
    }

    if (!entry.filenames) {
        entry.filenames = boost::make_shared<Entry::Filenames>();
    }
    entry.filenames->push_back(cover_uri_generic); 

    // AIMP 3.6 specific
    if (cover_hash) {
        cover_hash_entry_map_[*cover_hash] = entry;
    }
}

void GetCover::Cache::cacheBasedOnPreviousResult(TrackDescription track_desc, GetCover::Cache::SearchResult search_result, const int* cover_hash)
{
    Entry& entry = track_desc_entry_map_[track_desc];
    entry.filenames = search_result.entry->filenames;

    // AIMP 3.6 specific
    if (cover_hash) {
        cover_hash_entry_map_[*cover_hash] = entry;
    }
}

GetCover::Cache::SearchResult GetCover::Cache::isCoverCachedForCurrentTrack(TrackDescription track_desc, std::size_t width, std::size_t height) const
{
    // search in map current file covers of different sizes.
    const auto entry_it = track_desc_entry_map_.find(track_desc);
    if (track_desc_entry_map_.end() != entry_it) {
        if ( Cache::SearchResult r = findInEntryBySize(entry_it->second, width, height) ) {
            return r;
        }
    }

    return Cache::SearchResult();
}

GetCover::Cache::SearchResult GetCover::Cache::isCoverCachedForAnotherTrack(const boost::filesystem::wpath& cover_path, std::size_t width, std::size_t height) const
{
    // search in other cached track covers.
    assert(!cover_path.empty());

    auto track_it = path_track_map_.find(cover_path);
    if (path_track_map_.end() != track_it) {
        return isCoverCachedForCurrentTrack(track_it->second, width, height);
    } 

    return Cache::SearchResult();
}

GetCover::Cache::SearchResult GetCover::Cache::isCoverCachedForAnotherTrack(int hash, std::size_t width, std::size_t height) const
{
    auto it = cover_hash_entry_map_.find(hash);
    if (cover_hash_entry_map_.end() != it) {
        if ( Cache::SearchResult r = findInEntryBySize(it->second, width, height) ) {
            return r;
        }
    } 

    return Cache::SearchResult();
}

GetCover::Cache::SearchResult GetCover::Cache::findInEntryBySize(const Entry& cache_info, std::size_t width, std::size_t height) const
{
    //! search first entry of string "widthxheight" in filename string.
    struct MatchSize {
        MatchSize(std::size_t width, std::size_t height) {
            std::wostringstream s;
            s << width << "x" << height;
            string_to_find_ = s.str();
        }

        bool operator()(const std::wstring& filename) const
            { return filename.find(string_to_find_) != std::wstring::npos; }

        std::wstring string_to_find_;
    };

    const Entry::Filenames& names = *cache_info.filenames;
    // search in filenames.
    const auto filename_iter = std::find_if( names.begin(), names.end(), MatchSize(width, height) );
    if (names.end() != filename_iter) {
        // return pointer to found filename.
        return SearchResult(&cache_info, std::distance(names.begin(), filename_iter));
    }

    return SearchResult();
}

std::wstring GetCover::getTempFileNameForAlbumCover(TrackDescription track_desc, std::size_t width, std::size_t height)
{
    struct RandomDigitCharGenerator {
        RandomNumbersGenerator* gen_;

        RandomDigitCharGenerator(RandomNumbersGenerator* gen)
            : gen_(gen)
        {}

        wchar_t operator()() const
            { return static_cast<wchar_t>( L'0' + (*gen_)() ); }
    };

    // generate random part;
    std::generate(random_file_part_.begin(), random_file_part_.end(), RandomDigitCharGenerator(&die_) );

    std::wostringstream filename;
    filename << L"cover_" << track_desc.playlist_id << "_" << track_desc.track_id << L"_" << width << "x" << height << "_" << random_file_part_;
    return filename.str();
}

SubscribeOnAIMPStateUpdateEvent::EVENTS SubscribeOnAIMPStateUpdateEvent::getEventFromRpcParams(const Rpc::Value& params) const
{
    if (params.type() != Rpc::Value::TYPE_OBJECT && params.size() == 1) {
        throw Rpc::Exception("Wrong arguments count. Wait 1 string value: event ID.", WRONG_ARGUMENT);
    }

    const auto iter = event_types_.find(params["event"]);
    if ( iter != event_types_.end() ) {
        return iter->second; // event ID.
    }

    throw Rpc::Exception("Event does not supported", WRONG_ARGUMENT);
}

ResponseType SubscribeOnAIMPStateUpdateEvent::execute(const Rpc::Value& root_request, Rpc::Value& /*result*/)
{
    EVENTS event_id = getEventFromRpcParams(root_request["params"]);

    DelayedResponseSender_ptr comet_delayed_response_sender = rpc_request_handler_.getDelayedResponseSender();
    assert(comet_delayed_response_sender != nullptr);
    delayed_response_sender_descriptors_.insert( std::make_pair(event_id,
                                                                ResponseSenderDescriptor(root_request, comet_delayed_response_sender)
                                                                )
                                                );
    // TODO: add list of timers that remove comet_request_handler from event_handlers_ queue and send timeout response.
    return RESPONSE_DELAYED;
}

void SubscribeOnAIMPStateUpdateEvent::prepareResponse(EVENTS event_id, Rpc::Value& result) const
{
    switch (event_id)
    {
    case PLAYBACK_STATE_CHANGE_EVENT:
        RpcResultUtils::setCurrentPlaybackState(aimp_manager_.getPlaybackState(), result);
        RpcResultUtils::setCurrentTrackProgressIfPossible(aimp_manager_, result);
        break;
    case CURRENT_TRACK_CHANGE_EVENT:
        RpcResultUtils::setCurrentPlaylist(aimp_manager_.getPlayingPlaylist(), result);
        RpcResultUtils::setCurrentPlaylistEntry(aimp_manager_.getPlayingEntry(), result);
        break;
    case CONTROL_PANEL_STATE_CHANGE_EVENT:
        RpcResultUtils::setControlPanelInfo(aimp_manager_, result);
        if (aimp_app_is_exiting_) {
            result["aimp_app_is_exiting"] = true;
        }
        break;
    case PLAYLISTS_CONTENT_CHANGE_EVENT:
        RpcResultUtils::setPlaylistsContentChangeInfo(aimp_manager_, result);
        break;
    default:
        assert(!"Unsupported event ID in "__FUNCTION__);
        BOOST_LOG_SEV(logger(), error) << "Unsupported event ID " << event_id << " in "__FUNCTION__;
    }
}

void SubscribeOnAIMPStateUpdateEvent::aimpEventHandler(AIMPManager::EVENTS event)
{
    switch (event)
    {
    case AIMPManager::EVENT_TRACK_POS_CHANGED: // it's sent with period 1 second in AIMP2, useless.
        sendNotifications(CONTROL_PANEL_STATE_CHANGE_EVENT);
        break;
    case AIMPManager::EVENT_PLAY_FILE: // it's sent when playback started.
        sendNotifications(CURRENT_TRACK_CHANGE_EVENT);
        sendNotifications(CONTROL_PANEL_STATE_CHANGE_EVENT);
        break;
    case AIMPManager::EVENT_PLAYER_STATE:
        sendNotifications(PLAYBACK_STATE_CHANGE_EVENT);
        sendNotifications(CONTROL_PANEL_STATE_CHANGE_EVENT);
        break;
    case AIMPManager::EVENT_PLAYLISTS_CONTENT_CHANGE:
        sendNotifications(PLAYLISTS_CONTENT_CHANGE_EVENT);
        // if internet radio is playing and track is changed we must notify about it.
        if (   aimp_manager_.getPlaybackState() != AIMPManager::STOPPED
            && aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH) == 0
            ) 
        {
            sendNotifications(CURRENT_TRACK_CHANGE_EVENT);
            sendNotifications(CONTROL_PANEL_STATE_CHANGE_EVENT); // send notification about more general event than CURRENT_TRACK_CHANGE_EVENT, since web-client do not use CURRENT_TRACK_CHANGE_EVENT.
        }
        break;
    case AIMPManager::EVENT_TRACK_PROGRESS_CHANGED_DIRECTLY:
        sendNotifications(PLAYBACK_STATE_CHANGE_EVENT);
        break;
    
    case AIMPManager::EVENT_AIMP_QUIT:
        aimp_app_is_exiting_ = true;
    case AIMPManager::EVENT_VOLUME:
    case AIMPManager::EVENT_MUTE:
    case AIMPManager::EVENT_SHUFFLE:
    case AIMPManager::EVENT_REPEAT:
    case AIMPManager::EVENT_RADIO_CAPTURE:
        sendNotifications(CONTROL_PANEL_STATE_CHANGE_EVENT);
        break;
    default:
        break;
    }
}

void SubscribeOnAIMPStateUpdateEvent::sendNotifications(EVENTS event_id)
{
    std::pair<DelayedResponseSenderDescriptors::iterator, DelayedResponseSenderDescriptors::iterator> it_pair = delayed_response_sender_descriptors_.equal_range(event_id);
    //auto it_pair = delayed_response_sender_descriptors_.equal_range(event_id);
    for (DelayedResponseSenderDescriptors::iterator sender_it = it_pair.first,
                                                    end       = it_pair.second;
                                                    sender_it != end;
                                                    ++sender_it
         )
    {
        ResponseSenderDescriptor& sender_descriptor = sender_it->second;
        sendEventNotificationToSubscriber(event_id, sender_descriptor);
    }

    delayed_response_sender_descriptors_.erase(it_pair.first, it_pair.second);
}

void SubscribeOnAIMPStateUpdateEvent::sendEventNotificationToSubscriber(EVENTS event_id, ResponseSenderDescriptor& response_sender_descriptor)
{
    Rpc::Value response;
    prepareResponse(event_id, response["result"]); // TODO: catch errors here.
    response["id"] = response_sender_descriptor.root_request["id"];
    response_sender_descriptor.sender->sendResponseSuccess(response);
}

ResponseType GetPlayerControlPanelState::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    RpcResultUtils::setControlPanelInfo(aimp_manager_, root_response["result"]);
    return RESPONSE_IMMEDIATE;
}

ResponseType SetTrackRating::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];

    const TrackDescription track_desc(getTrackDesc(params));
    const int rating( Utilities::limit_value<int>(params["rating"], 0, 5) ); // set rating range [0, 5]

    IPlaylistEntryRatingManager* rating_manager = dynamic_cast<IPlaylistEntryRatingManager*>(&aimp_manager_);
    if (rating_manager) {
        try {
            rating_manager->trackRating(track_desc, rating);
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error saving rating in "__FUNCTION__". Reason: " << e.what();
            throw Rpc::Exception("Error saving rating.", RATING_SET_FAILED);
        }
    } else {
        // AIMP2 does not support rating set. Save rating in simple text file.
        try {
            std::wofstream file(file_to_save_ratings_, std::ios_base::out | std::ios_base::app);
            file.imbue( std::locale("") ); // set system locale.
            if ( file.good() ) {
                const std::wstring& entry_filename = getEntryField<std::wstring>(AIMPPlayer::getPlaylistsDB(aimp_manager_), "filename", track_desc);
                file << entry_filename << L"; rating:" << rating << L"\n";
                file.close();
            } else {
                throw std::exception("Ratings file can not be opened.");
            }
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error saving rating to text file in "__FUNCTION__". Reason: " << e.what();
            throw Rpc::Exception("Error saving rating to text file.", RATING_SET_FAILED);
        }
    }

    root_response["rating"] = rating; 
    return RESPONSE_IMMEDIATE;
}

ResponseType Version::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    Rpc::Value& result = root_response["result"];
    result["aimp_version"] = aimp_manager_.getAIMPVersion();

    std::string plugin_version;
    try {
        using namespace Utilities;
        plugin_version = StringEncoding::utf16_to_utf8( Utilities::getPluginVersion() );
    } catch (...) {
        plugin_version = "unknown"; 
    }
    result["plugin_version"] = plugin_version;
    return RESPONSE_IMMEDIATE;
}

ResponseType PluginCapabilities::execute(const Rpc::Value& /*root_request*/, Rpc::Value& root_response)
{
    Rpc::Value& result = root_response["result"];
    result["upload_track"] = ControlPlugin::AIMPControlPlugin::settings().misc.enable_track_upload;
    result["physical_track_deletion"] = ControlPlugin::AIMPControlPlugin::settings().misc.enable_physical_track_deletion;
    result["scheduler"] = ControlPlugin::AIMPControlPlugin::settings().misc.enable_scheduler;
    return RESPONSE_IMMEDIATE;
}

ResponseType AddURLToPlaylist::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    const PlaylistID playlist_id = params["playlist_id"];
    const std::string& url = params["url"];

    try {
        aimp_manager_.addURLToPlaylist(url, playlist_id);
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Adding track to playlist failed", ADD_URL_TO_PLAYLIST_FAILED);
    }

    Rpc::Value& result = root_response["result"];
    result = Rpc::Value::Object();
    return RESPONSE_IMMEDIATE;
}

RemoveTrack::RemoveTrack(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler, boost::asio::io_service& io_service)
    : AIMPRPCMethod("RemoveTrack", aimp_manager, rpc_request_handler),
      track_deletion_timer_(io_service)
{
    const ControlPlugin::PluginSettings::Settings& settings = ControlPlugin::AIMPControlPlugin::settings();
    enable_physical_track_deletion_ = settings.misc.enable_physical_track_deletion;
}

ResponseType RemoveTrack::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];

    const bool physically = params.isMember("physically") ? params["physically"] : false;
    if (physically && !enable_physical_track_deletion_) {
        throw Rpc::Exception("Physical deletion is disabled", REMOVE_TRACK_PHYSICAL_DELETION_DISABLED);
    } 

    const TrackDescription track_desc(getTrackDesc(params));
    bool remove_track_manually = aimp_manager_.getAbsoluteTrackDesc(track_desc) == aimp_manager_.getPlayingTrack();
    fs::path filename_to_delete;
    if (physically && remove_track_manually) {
        try {
            filename_to_delete = aimp_manager_.getEntryFilename(track_desc);
        } catch (std::exception&) {
            remove_track_manually = false;
        }
    }

    try {
        aimp_manager_.removeTrack(track_desc, physically && !remove_track_manually);
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Removing track failed", REMOVE_TRACK_FAILED);
    }

    if (filename_to_delete.empty()) {
        Rpc::Value& result = root_response["result"];
        result = Rpc::Value::Object();
        return RESPONSE_IMMEDIATE;
    } else {
        aimp_manager_.playNextTrack();
        // TODO: also here we should check if there is only one track in all playlists to stop playback.

        DelayedResponseSender_ptr comet_delayed_response_sender = rpc_request_handler_.getDelayedResponseSender();
        assert(comet_delayed_response_sender != nullptr);

        track_deletion_timer_.expires_from_now( boost::posix_time::seconds(kTRACK_DELETION_DELAY_SEC) );
        track_deletion_timer_.async_wait( boost::bind( &RemoveTrack::onTimerDeleteTrack, this, filename_to_delete, ResponseSenderDescriptor(root_request, comet_delayed_response_sender), _1 ) );
        return RESPONSE_DELAYED;
    }
}

void RemoveTrack::onTimerDeleteTrack(const fs::path& filename, RemoveTrack::ResponseSenderDescriptor& response_sender_descriptor, const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        boost::system::error_code ec;
        fs::remove(filename, ec);
        if (!ec) {
            Rpc::Value response;
            response["result"] = Rpc::Value::Object();
            response["id"] = response_sender_descriptor.root_request["id"];
            response_sender_descriptor.sender->sendResponseSuccess(response);
            return;
        }

        BOOST_LOG_SEV(logger(), error) << "Error of fs::remove in "__FUNCTION__". File: " << StringEncoding::utf16_to_utf8(filename.native()) << ". Reason: " << ec;
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }

    response_sender_descriptor.sender->sendResponseFault(response_sender_descriptor.root_request, "Removing track failed", REMOVE_TRACK_FAILED);
}

Scheduler::Scheduler(AIMPManager& aimp_manager, Rpc::RequestHandler& rpc_request_handler, boost::asio::io_service& io_service)
    : AIMPRPCMethod("Scheduler", aimp_manager, rpc_request_handler),
      io_service_(io_service)
{
    const ControlPlugin::PluginSettings::Settings& settings = ControlPlugin::AIMPControlPlugin::settings();
    enable_scheduler_ = settings.misc.enable_scheduler;
}

ResponseType Scheduler::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    if (!enable_scheduler_) {
        throw Rpc::Exception("Scheduler is disabled", SCHEDULER_DISABLED);
    }

    const Rpc::Value& params = root_request["params"];

    const bool cancel_timer = params.isMember("cancel");
    if (cancel_timer) {
        timer_.reset();
    }

    std::string action = !cancel_timer && params.isMember("action") ? static_cast<std::string>(params["action"]) : "";
    if (!action.empty()) {
        std::unique_ptr<Timer> timer(new Timer(action, io_service_));

        if (params.isMember("expiration_time") && !params.isMember("expiration_delay")) {
            double expiration_time = params["expiration_time"];
            timer->expires_at(expiration_time);
        } else if (params.isMember("expiration_delay") && !params.isMember("expiration_time")) {
            double expiration_delay = params["expiration_delay"];
            timer->timer_.expires_from_now(boost::posix_time::seconds(static_cast<long>(expiration_delay)));            
        } else {
            throw Rpc::Exception("Schedule action failed. Reason: expiration_time or expiration_delay is required.", WRONG_ARGUMENT);
        }

        if (action == "stop_playback") {
            timer->timer_.async_wait( boost::bind( &Scheduler::onTimerStopPlayback, this, _1 ) );
        } else if (action == "pause_playback") {
            timer->timer_.async_wait( boost::bind( &Scheduler::onTimerPausePlayback, this, _1 ) );
        } else if (action == "machine_shutdown") {
            if (!PowerManagement::ShutdownEnabled()) {
                throw Rpc::Exception("Schedule action failed. Reason: shutdown is disabled on this machine.", SCHEDULER_UNSUPPORTED_ACTION);
            }

            timer->timer_.async_wait( boost::bind( &Scheduler::onTimerMachineShutdown, this, _1 ) );
        } else if (action == "machine_hibernate") {
            if (!PowerManagement::HibernationEnabled()) {
                throw Rpc::Exception("Schedule action failed. Reason: hibernation is disabled on this machine.", SCHEDULER_UNSUPPORTED_ACTION);
            }

            timer->timer_.async_wait( boost::bind( &Scheduler::onTimerMachineHibernate, this, _1 ) );
        } else if (action == "machine_sleep") {
            if (!PowerManagement::SleepEnabled()) {
                throw Rpc::Exception("Schedule action failed. Reason: sleep mode is disabled on this machine.", SCHEDULER_UNSUPPORTED_ACTION);
            }

            timer->timer_.async_wait( boost::bind( &Scheduler::onTimerMachineSleep, this, _1 ) );
        } else {
            throw Rpc::Exception("Schedule action failed. Reason: unsupported action.", SCHEDULER_UNSUPPORTED_ACTION);
        }

        timer_.swap(timer);
    }

    Rpc::Value& result = root_response["result"];

    Rpc::Value& supported_actions = result["supported_actions"];
    supported_actions.setSize(2);
    supported_actions[0] = "stop_playback";
    supported_actions[1] = "pause_playback";

    if (PowerManagement::ShutdownEnabled()) {
        supported_actions.setSize(supported_actions.size() + 1);
        supported_actions[supported_actions.size() - 1] = "machine_shutdown";
    }

    if (PowerManagement::SleepEnabled()) {
        supported_actions.setSize(supported_actions.size() + 1);
        supported_actions[supported_actions.size() - 1] = "machine_sleep";
    }

    if (PowerManagement::HibernationEnabled()) {
        supported_actions.setSize(supported_actions.size() + 1);
        supported_actions[supported_actions.size() - 1] = "machine_hibernate";
    }

    // report current timer state.
    if (timer_) {
        Rpc::Value& current_timer = result["current_timer"];
        current_timer["action"] = timer_->action();
        current_timer["expires_at"] = timer_->expires_at();
        current_timer["expires_in"] = timer_->expires_in();
    }

    return RESPONSE_IMMEDIATE;
}

void Scheduler::onTimerStopPlayback(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        aimp_manager_.stopPlayback();

        BOOST_LOG_SEV(logger(), info) << "playback has been stopped by timer. "__FUNCTION__;

        timer_.reset();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

void Scheduler::onTimerPausePlayback(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.

        if (aimp_manager_.getPlaybackState() == AIMPManager::PLAYBACK_STATE::PLAYING) {
            aimp_manager_.pausePlayback();
            BOOST_LOG_SEV(logger(), info) << "playback has been paused by timer. "__FUNCTION__;
        } else {
            BOOST_LOG_SEV(logger(), info) << "Pausing playback by timer has been skipped because of player state " << aimp_manager_.getPlaybackState() << " in "__FUNCTION__;
        }

        timer_.reset();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

void Scheduler::onTimerMachineShutdown(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        if (PowerManagement::SystemShutdown()) {
            BOOST_LOG_SEV(logger(), info) << "Machine has been shut down by timer. "__FUNCTION__;
        }

        timer_.reset();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

void Scheduler::onTimerMachineHibernate(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        if (PowerManagement::SystemHibernate()) {
            BOOST_LOG_SEV(logger(), info) << "Machine has been hibernated by timer. "__FUNCTION__;
        }

        timer_.reset();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

void Scheduler::onTimerMachineSleep(const boost::system::error_code& e)
{
    if (!e) { // Timer expired normally.
        if (PowerManagement::SystemSleep()) {
            BOOST_LOG_SEV(logger(), info) << "Machine has been set to sleep mode by timer. "__FUNCTION__;
        }

        timer_.reset();
    } else if (e != boost::asio::error::operation_aborted) { // "operation_aborted" error code is sent when timer is cancelled.
        BOOST_LOG_SEV(logger(), error) << "err:"__FUNCTION__" timer error:" << e;
    }
}

Scheduler::Timer::Timer(std::string action, boost::asio::io_service& io_service) 
    :
    action_(action),
    timer_(io_service)
{
}

void Scheduler::Timer::expires_at(double unix_time)
{
    boost::posix_time::ptime time = boost::posix_time::from_time_t(static_cast<time_t>(unix_time));   
    timer_.expires_at(time);
}

int64_t toPosix64(const boost::posix_time::ptime& pt)
{
  using namespace boost::posix_time;
  static ptime epoch(boost::gregorian::date(1970, 1, 1));
  time_duration diff(pt - epoch);
  return (diff.ticks() / diff.ticks_per_second());
}

double Scheduler::Timer::expires_at() const
{ 
    return static_cast<double>( toPosix64(timer_.expires_at()) );
}

double Scheduler::Timer::expires_in() const
{ 
    boost::posix_time::time_duration diff(timer_.expires_at() - boost::posix_time::second_clock::universal_time());
    return static_cast<double>(diff.total_seconds());
}

} // namespace AimpRpcMethods
