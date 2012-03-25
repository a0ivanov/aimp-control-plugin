// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "methods.h"
#include "aimp/manager.h"
#include "aimp/manager3_impl.h"
#include "plugin/logger.h"
#include "rpc/exception.h"
#include "rpc/value.h"
#include "rpc/request_handler.h"
#include "utils/util.h"
#include "utils/string_encoding.h"
#include <fstream>
#include <boost/range.hpp>
#include <boost/bind.hpp>
#include <boost/assign/std.hpp>
#include <boost/foreach.hpp>
#include <string.h>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Rpc::RequestHandler>(); }
}

namespace AimpRpcMethods
{

using namespace Rpc;

typedef AIMPManager::PlaylistsListType PlaylistsListType;

/*!
    \brief Returns reference to Playlist object by playlist ID.
    Helper function.
*/
const Playlist& getPlayListFromRpcParam(const AIMPManager& aimp_manager, int playlist_id) // throws Rpc::Exception
{
    try {
        return aimp_manager.getPlaylist(playlist_id);
    } catch (std::runtime_error&) {
        std::ostringstream msg;
        msg << "Playlist with ID = " << playlist_id << " does not exist.";
        throw Rpc::Exception(msg.str(), PLAYLIST_NOT_FOUND);
    }
}

ResponseType Play::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() == 2) {
        const TrackDescription track_desc(params["playlist_id"], params["track_id"]);
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

    RpcResultUtils::setCurrentPlayingSourceInfo(aimp_manager_, root_response["result"]); // return current playlist and track.
    return RESPONSE_IMMEDIATE;
}

ResponseType GetFormattedEntryTitle::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() == Rpc::Value::TYPE_OBJECT && params.size() != 3) {
        throw Rpc::Exception("Wrong arguments count. Wait 3 arguments: int track_id, int playlist_id, string format_string", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);
    try {
        const PlaylistEntry& entry = aimp_manager_.getEntry(track_desc);
        using namespace StringEncoding;
        root_response["result"]["formatted_string"] = utf16_to_utf8( aimp_manager_.getFormattedEntryTitle(entry, params["format_string"]) );
    } catch(std::runtime_error&) {
        throw Rpc::Exception("Specified track does not exist.", TRACK_NOT_FOUND);
    } catch (std::invalid_argument&) {
        throw Rpc::Exception("Wrong specified format string", WRONG_ARGUMENT);
    }

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
            } catch (std::runtime_error& e) {
                throw Rpc::Exception(e.what(), STATUS_SET_FAILED);
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

ResponseType EnqueueTrack::execute(const Rpc::Value& root_request, Rpc::Value& /*root_response*/)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() < 2) {
        throw Rpc::Exception("Wrong arguments count. Wait at least 2 int values: track_id, playlist_id.", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);
    const bool insert_at_queue_beginning = params.isMember("insert_at_queue_beginning") ? bool(params["insert_at_queue_beginning"]) 
                                                                                        : false; // by default insert at the end of queue.

    try {
        aimp_manager_.enqueueEntryForPlay(track_desc, insert_at_queue_beginning);
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Enqueue track failed. Reason: internal AIMP error.", ENQUEUE_TRACK_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

ResponseType RemoveTrackFromPlayQueue::execute(const Rpc::Value& root_request, Rpc::Value& /*root_response*/)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() != 2) {
        throw Rpc::Exception("Wrong arguments count. Wait 2 int values: track_id, playlist_id.", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);
    try {
        aimp_manager_.removeEntryFromPlayQueue(track_desc);
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Removing track from play queue failed. Reason: internal AIMP error.", DEQUEUE_TRACK_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

ResponseType GetPlaylists::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    const AIMPManager::PlaylistsListType& playlists = aimp_manager_.getPlayLists();

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

    Rpc::Value& playlists_rpcvalue = root_response["result"];
    playlists_rpcvalue.setSize( playlists.size() );

    // fill rpcvalue array of playlists.
    size_t playlist_index = 0;
    BOOST_FOREACH(const auto& playlist_id_obj_pair, playlists) {
        const Playlist& playlist = playlist_id_obj_pair.second;
        Rpc::Value& playlist_rpcvalue = playlists_rpcvalue[playlist_index];
        // fill all requested fields for playlist.
        playlist_fields_filler_.fillRpcArrayOfObjects(playlist, playlist_rpcvalue);
        ++playlist_index;
    }
    return RESPONSE_IMMEDIATE;
}

const size_t GetPlaylistEntriesTemplateMethod::getStartFromIndexFromRpcParam(int start_from_index, size_t max_value) // throws Rpc::Exception
{
    if (start_from_index < 0 || static_cast<size_t>(start_from_index) > max_value) {
        throw Rpc::Exception("Wrong argument: start_index out of bound.", WRONG_ARGUMENT);
    }

    return static_cast<size_t>(start_from_index);
}

const size_t GetPlaylistEntriesTemplateMethod::getEntriesCountFromRpcParam(int entries_count, size_t max_value) // throws Rpc::Exception
{
    if (entries_count == -1) {
        // -1 is special value which means "all available items". Included to support jQuery Datatables 1.7.6.
        entries_count = max_value;
    } else if (entries_count < 0) {
        throw Rpc::Exception("Wrong argument: entries_count should be positive value or -1(for request all entries).", WRONG_ARGUMENT);
    }

    return static_cast<size_t>(entries_count) < max_value ? static_cast<size_t>(entries_count)
                                                          : max_value;
}

bool GetPlaylistEntriesTemplateMethod::getSearchStringFromRpcParam(const std::string& search_string_utf8)
{
    using namespace StringEncoding;
    try {
        search_string_ = utf8_to_utf16(search_string_utf8);
    } catch (EncodingException& e) {
        BOOST_LOG_SEV(logger(), error) << "Error of convertation UTF8 search string to UTF16 in "__FUNCTION__". Reason: " << e.what();
        search_string_.clear();
    }

    return !search_string_.empty();
}

void GetPlaylistEntriesTemplateMethod::fillFieldToOrderDescriptors(const Rpc::Value& entry_fields_to_order)
{
    using namespace EntriesSortUtil;
    field_to_order_descriptors_.clear();
    const size_t fields_count = entry_fields_to_order.size();
    
    for (size_t field_index = 0; field_index < fields_count; ++field_index) {
       const Rpc::Value& field_desc = entry_fields_to_order[field_index];
        try {
            const std::string& field_to_order = field_desc[kFIELD_STRING];
            
            const auto supported_field_it = fields_to_order_.find(field_to_order);
            if ( supported_field_it != fields_to_order_.end() ) {
                // TODO: avoid duplicates
                field_to_order_descriptors_.push_back( FieldToOrderDescriptor( supported_field_it->second,
                                                                               (field_desc[kORDER_DIRECTION_STRING] == kDESCENDING_ORDER_STRING) ? DESCENDING 
                                                                                                                                                 : ASCENDING
                                                                              )
                                                      );
            } else {
                std::ostringstream msg;
                msg << "Wrong argument: ordering by field " << field_to_order << " is not supported.";
                throw Rpc::Exception(msg.str(), WRONG_ARGUMENT);
            }
        } catch (Rpc::Exception&) {
            // just ignore wrong order field description, ordering is not necessary.
        }
    }
}

const PlaylistEntryIDList& GetPlaylistEntriesTemplateMethod::getEntriesIDsFilteredByStringFromEntriesList(const std::wstring& search_string,
                                                                                                          const EntriesListType& entries)
{
    filtered_entries_ids_.clear();
    filtered_entries_ids_.reserve( entries.size() );

    size_t entry_index = 0;
    BOOST_FOREACH (const PlaylistEntry& entry, entries) {
        if ( entry_contain_string_(entry, search_string) ) {
            filtered_entries_ids_.push_back(entry_index);
        }
        ++entry_index;
    }

    return filtered_entries_ids_;
}

const PlaylistEntryIDList& GetPlaylistEntriesTemplateMethod::getEntriesIDsFilteredByStringFromEntryIDs(const std::wstring& search_string,
                                                                                                       const PlaylistEntryIDList& entry_to_filter_ids,
                                                                                                       const EntriesListType& entries)
{
    filtered_entries_ids_.clear();
    filtered_entries_ids_.reserve( entry_to_filter_ids.size() );
    BOOST_FOREACH (const PlaylistEntryID entry_id, entry_to_filter_ids) {
        if ( entry_contain_string_(entries[entry_id], search_string) ) {
            filtered_entries_ids_.push_back(entry_id);
        }
    }

    return filtered_entries_ids_;
}

void GetPlaylistEntriesTemplateMethod::handleFilteredEntryIDs(const PlaylistEntryIDList& filtered_entries_ids, const EntriesListType& entries,
                                                             size_t start_entry_index, size_t entries_count,
                                                             EntryIDsHandler entry_ids_handler,
                                                             EntryIDsHandler full_entry_ids_list_handler,
                                                             EntriesCountHandler filtered_entries_count_handler)
{
    filtered_entries_count_handler( filtered_entries_ids.size() );

    const size_t filtered_start_entry_index = std::min(start_entry_index,
                                                       filtered_entries_ids.empty() ? 0 
                                                                                    : filtered_entries_ids.size() - 1
                                                       );
    const size_t filtered_entries_count = std::min(entries_count,
                                                   filtered_entries_ids.size() - filtered_start_entry_index);
    handleEntryIDs(filtered_entries_ids, entries,
                   filtered_start_entry_index, filtered_entries_count,
                   entry_ids_handler,
                   full_entry_ids_list_handler);
}

void GetPlaylistEntriesTemplateMethod::handleEntries(const EntriesListType& entries, size_t start_entry_index, size_t entries_count,
                                                     EntriesHandler entries_handler, 
                                                     EntriesHandler full_entries_list_handler)
{
    full_entries_list_handler( EntriesRange( entries.begin(), entries.end() ) );

    EntriesListType::const_iterator entries_iterator_begin( entries.begin() );
    std::advance(entries_iterator_begin, start_entry_index); // go to first requested entry.
    EntriesListType::const_iterator entries_iterator_end(entries_iterator_begin);
    std::advance(entries_iterator_end, entries_count); // go to entry next to last requested.
    EntriesRange entries_range(entries_iterator_begin, entries_iterator_end);
    
    entries_handler(entries_range);
}

void GetPlaylistEntriesTemplateMethod::handleEntryIDs(const PlaylistEntryIDList& entries_ids, const EntriesListType& entries,
                                                      size_t start_entry_index, size_t entries_count,
                                                      EntryIDsHandler entry_ids_handler,
                                                      EntryIDsHandler full_entry_ids_list_handler
                                                      )
{
    assert(entries_ids.size() >= start_entry_index + entries_count);

    full_entry_ids_list_handler(EntriesIDsRange( entries_ids.begin(), 
                                                 entries_ids.end() 
                                                ),
                                entries);

    PlaylistEntryIDList::const_iterator entry_id_iter_begin( entries_ids.begin() );
    std::advance(entry_id_iter_begin, start_entry_index); // go to first requested entry.
    PlaylistEntryIDList::const_iterator entry_id_iter_end(entry_id_iter_begin);
    std::advance(entry_id_iter_end, entries_count); // go to entry next to last requested.
    EntriesIDsRange entry_ids_range(entry_id_iter_begin, entry_id_iter_end);
    
    entry_ids_handler(entry_ids_range, entries);
}

ResponseType GetPlaylistEntriesTemplateMethod::execute(const Rpc::Value& params,
                                                       // following needed by only by GetPlaylistEntries
                                                       EntriesHandler entries_handler,
                                                       EntryIDsHandler entry_ids_handler,
                                                       EntriesCountHandler total_entries_count_handler,
                                                       EntriesCountHandler filtered_entries_count_handler,
                                                       // following needed by only by GetEntryPositionInDataTable
                                                       EntriesHandler full_entries_list_handler,
                                                       EntryIDsHandler full_entry_ids_list_handler,
                                                       EntriesCountHandler page_size_handler
                                                       )
{
    // ensure we got obligatory argument: playlist id.
    if (params.size() < 1) {
        throw Rpc::Exception("Wrong arguments count. Wait at least int 'playlist_id' argument.", WRONG_ARGUMENT);
    }

    const Playlist& playlist = getPlayListFromRpcParam(aimp_manager_, params["playlist_id"]);
    const EntriesListType& entries = playlist.entries();

    total_entries_count_handler( entries.size() );

    const size_t start_entry_index = params.isMember("start_index") ? getStartFromIndexFromRpcParam(params["start_index"],
                                                                                                    (entries.size() == 0) ? 0
                                                                                                                          : entries.size() - 1
                                                                                                    )
                                                                    : 0; // by default return entries from start.

    const size_t entries_count = params.isMember(kENTRIES_COUNT_STRING) ? getEntriesCountFromRpcParam(params[kENTRIES_COUNT_STRING],
                                                                                                      entries.size() - start_entry_index
                                                                                                      )
                                                                        : entries.size() - start_entry_index; // by default return entries from start_entry_index to end of entries list.

    if ( params.isMember(kENTRIES_COUNT_STRING) ) {
        const int entries_count = params[kENTRIES_COUNT_STRING];
        page_size_handler( static_cast<size_t>(entries_count) );
    }

    if ( !params.isMember(kORDER_FIELDS_STRING) && !params.isMember(kSEARCH_STRING_STRING) ) { // return entries in default order.
        handleEntries(entries, start_entry_index, entries_count, entries_handler, full_entries_list_handler);
    } else if ( params.isMember(kORDER_FIELDS_STRING) ) { // return entries in specified order if order descriptors are valid.
        fillFieldToOrderDescriptors(params[kORDER_FIELDS_STRING]);
        if ( !field_to_order_descriptors_.empty() ) { // return entries in specified order.
            // get entries ids in specified ordering.
            const PlaylistEntryIDList& sorted_entries_ids = (field_to_order_descriptors_.size() == 1) ? playlist.getEntriesSortedByField(field_to_order_descriptors_[0])
                                                                                                      : playlist.getEntriesSortedByMultipleFields(field_to_order_descriptors_)
            ;

            if ( params.isMember(kSEARCH_STRING_STRING) && getSearchStringFromRpcParam(params[kSEARCH_STRING_STRING]) ) { // return entries in specified order and filtered by search string.
                handleFilteredEntryIDs(getEntriesIDsFilteredByStringFromEntryIDs(search_string_, sorted_entries_ids, entries),
                                       entries,
                                       start_entry_index,
                                       entries_count,
                                       entry_ids_handler,
                                       full_entry_ids_list_handler,
                                       filtered_entries_count_handler
                                      );
            } else { // return entries in specified order.
                handleEntryIDs(sorted_entries_ids, entries, start_entry_index, entries_count, entry_ids_handler, full_entry_ids_list_handler);
            }
        } else { // return entries in default order.
            if ( params.isMember(kSEARCH_STRING_STRING) && getSearchStringFromRpcParam(params[kSEARCH_STRING_STRING]) ) { // return entries in default order and filtered by search string.
                handleFilteredEntryIDs(getEntriesIDsFilteredByStringFromEntriesList(search_string_, entries),
                                       entries,
                                       start_entry_index,
                                       entries_count,
                                       entry_ids_handler,
                                       full_entry_ids_list_handler,
                                       filtered_entries_count_handler
                                      );
            } else { // return entries in default order.
                handleEntries(entries, start_entry_index, entries_count, entries_handler, full_entries_list_handler);
            }
        }
    } else if ( params.isMember(kSEARCH_STRING_STRING) && getSearchStringFromRpcParam(params[kSEARCH_STRING_STRING]) ) {
        handleFilteredEntryIDs(getEntriesIDsFilteredByStringFromEntriesList(search_string_, entries),
                               entries,
                               start_entry_index,
                               entries_count,
                               entry_ids_handler,
                               full_entry_ids_list_handler,
                               filtered_entries_count_handler
                               );
    }
    return RESPONSE_IMMEDIATE;
}

struct EntriesHandlerStub {
    void operator()(const EntriesRange& /*range*/) const {}
};

struct EntryIDsHandlerStub {
    void operator()(const EntriesIDsRange& /*range*/, const EntriesListType& /*entries*/) const {}
};

struct EntriesCountHandlerStub {
    void operator()(size_t /*count*/) const {}
};

struct Formatter {
    const AIMPManager* aimp_manager_;
    const std::string* format_string_;
    Formatter(const AIMPManager* aimp_manager, const std::string* format_string)
        :
        aimp_manager_(aimp_manager),
        format_string_(format_string)
    {}

    void operator()(const PlaylistEntry& entry, Rpc::Value& rpc_value) const { 
        rpc_value = StringEncoding::utf16_to_utf8( aimp_manager_->getFormattedEntryTitle(entry, 
                                                                                         *format_string_)
                                                  );
    }
};

GetPlaylistEntries::GetPlaylistEntries(AIMPManager& aimp_manager,
                                       Rpc::RequestHandler& rpc_request_handler
                                       )
    :
    AIMPRPCMethod("GetPlaylistEntries", aimp_manager, rpc_request_handler),
    get_playlist_entries_templatemethod_( new GetPlaylistEntriesTemplateMethod(aimp_manager) ),
    entry_fields_filler_("entry"),
    kFORMAT_STRING_STRING("format_string"),
    kFIELDS_STRING("fields"),
    kTOTAL_ENTRIES_COUNT_STRING("total_entries_count"),
    kENTRIES_STRING("entries"),
    kCOUNT_OF_FOUND_ENTRIES_STRING("count_of_found_entries")
{

    using namespace RpcValueSetHelpers;
    using namespace RpcResultUtils;
    boost::assign::insert(entry_fields_filler_.setters_)
        ( getStringFieldID(PlaylistEntry::ID),       boost::bind( createSetter(&PlaylistEntry::id),       _1, _2 ) )  // Use plugin id of entry instead Aimp internal id( PlaylistEntry::trackID() ).
        ( getStringFieldID(PlaylistEntry::TITLE),    boost::bind( createSetter(&PlaylistEntry::title),    _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::ARTIST),   boost::bind( createSetter(&PlaylistEntry::artist),   _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::ALBUM),    boost::bind( createSetter(&PlaylistEntry::album),    _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::DATE),     boost::bind( createSetter(&PlaylistEntry::date),     _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::GENRE),    boost::bind( createSetter(&PlaylistEntry::genre),    _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::BITRATE),  boost::bind( createSetter(&PlaylistEntry::bitrate),  _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::DURATION), boost::bind( createSetter(&PlaylistEntry::duration), _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::FILESIZE), boost::bind( createSetter(&PlaylistEntry::fileSize), _1, _2 ) )
        ( getStringFieldID(PlaylistEntry::RATING),   boost::bind( createSetter(&PlaylistEntry::rating),   _1, _2 ) )
    ;

    // fill supported field names.
    PlaylistEntries::SupportedFieldNames fields_names;
    BOOST_FOREACH(auto& setter_it, entry_fields_filler_.setters_) {
        fields_names.insert(setter_it.first);
    }
    get_playlist_entries_templatemethod_->setSupportedFieldNames(fields_names);

    // create handlers for passing into get_playlist_entries_templatemethod_'s execute().
    entries_handler_stub_       = boost::bind<void>(EntriesHandlerStub(), _1);
    enties_ids_handler_stub_    = boost::bind<void>(EntryIDsHandlerStub(), _1, _2);
    entries_count_handler_stub_ = boost::bind<void>(EntriesCountHandlerStub(), _1);
}

void GetPlaylistEntries::fillRpcValueEntriesFromEntriesList(EntriesRange entries_range,
                                                            Rpc::Value& rpcvalue_entries)
{
    rpcvalue_entries.setSize( entries_range.size() );

    size_t entry_rpcvalue_index = 0;
    BOOST_FOREACH (const PlaylistEntry& entry, entries_range) {
        Rpc::Value& entry_rpcvalue = rpcvalue_entries[entry_rpcvalue_index];
        // fill all requested fields for entry.
        entry_fields_filler_.fillRpcArrayOfArrays(entry, entry_rpcvalue);
        ++entry_rpcvalue_index;
    }
}

void GetPlaylistEntries::fillRpcValueEntriesFromEntryIDs(EntriesIDsRange entries_ids_range, const EntriesListType& entries,
                                                         Rpc::Value& rpcvalue_entries)
{
    rpcvalue_entries.setSize( entries_ids_range.size() );

    size_t entry_rpcvalue_index = 0;
    BOOST_FOREACH (const PlaylistEntryID entry_id, entries_ids_range) {
        Rpc::Value& entry_rpcvalue = rpcvalue_entries[entry_rpcvalue_index];
        // fill all requested fields for entry.
        entry_fields_filler_.fillRpcArrayOfArrays(entries[entry_id], entry_rpcvalue);
        ++entry_rpcvalue_index;
    }
}

void GetPlaylistEntries::initEntriesFiller(const Rpc::Value& params)
{
    if ( params.isMember(kFORMAT_STRING_STRING) ) { // 'format_string' param has priority over 'fields' param.
        typedef RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry>::RpcValueSetters RpcValueSetters;
        RpcValueSetters::iterator setter_it = entry_fields_filler_.setters_.find(kFORMAT_STRING_STRING);
        if ( setter_it == entry_fields_filler_.setters_.end() ) {
            // add filler if it does not exist yet.
            setter_it = entry_fields_filler_.setters_.insert( std::make_pair(kFORMAT_STRING_STRING,
                                                              RpcValueSetHelpers::HelperFillRpcFields<PlaylistEntry>::RpcValueSetter()
                                                                             )
                                                             ).first;
        }
        
        const std::string& format_string = params[kFORMAT_STRING_STRING];
        setter_it->second = boost::bind<void>(Formatter(&aimp_manager_, &format_string),
                                              _1, _2
                                              );
        entry_fields_filler_.setters_required_.clear();
        entry_fields_filler_.setters_required_.push_back(setter_it);
    } else {
        if ( params.isMember(kFIELDS_STRING) ) {
            entry_fields_filler_.initRequiredFieldsHandlersList(params[kFIELDS_STRING]);
        } else {
            // set default fields
            Rpc::Value fields;
            fields.setSize(2);
            fields[0] = "id";
            fields[1] = "title";
            entry_fields_filler_.initRequiredFieldsHandlersList(fields);
        }
    }
}

Rpc::ResponseType GetPlaylistEntries::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    PROFILE_EXECUTION_TIME(__FUNCTION__);

    const Rpc::Value& rpc_params = root_request["params"];

    initEntriesFiller(rpc_params);

    Rpc::Value& rpc_result = root_response["result"];
    Rpc::Value& rpcvalue_entries = rpc_result[kENTRIES_STRING];

    struct SetEntriesCount {
        mutable Rpc::Value* value_;
        const std::string* member_name_;
        SetEntriesCount(Rpc::Value& value, const std::string& member_name)
            : value_(&value),
              member_name_(&member_name)
        {}
        void operator()(size_t count) const
            { (*value_)[*member_name_] = count; }
    };

    Rpc::ResponseType response_type = get_playlist_entries_templatemethod_->execute(rpc_params, 
                                                                                    boost::bind(&GetPlaylistEntries::fillRpcValueEntriesFromEntriesList,
                                                                                                this, _1, boost::ref(rpcvalue_entries)
                                                                                                ),
                                                                                    boost::bind(&GetPlaylistEntries::fillRpcValueEntriesFromEntryIDs,
                                                                                                this, _1, _2, boost::ref(rpcvalue_entries)
                                                                                                ),
                                                                                    boost::bind<void>(SetEntriesCount(rpc_result, kTOTAL_ENTRIES_COUNT_STRING), _1),
                                                                                    boost::bind<void>(SetEntriesCount(rpc_result, kCOUNT_OF_FOUND_ENTRIES_STRING), _1),
                                                                                    // use stubs instead not used callbacks.
                                                                                    entries_handler_stub_,
                                                                                    enties_ids_handler_stub_,
                                                                                    entries_count_handler_stub_
                                                                                    );
    return response_type;
}

GetEntryPositionInDataTable::GetEntryPositionInDataTable(AIMPManager& aimp_manager,
                                                         Rpc::RequestHandler& rpc_request_handler,
                                                         GetPlaylistEntries& getplaylistentries_method
                                                         )
    :
    AIMPRPCMethod("GetEntryPositionInDataTable", aimp_manager, rpc_request_handler),
    get_playlist_entries_templatemethod_( getplaylistentries_method.getPlaylistEntriesTemplateMethod() )
{
    // create handlers for passing into get_playlist_entries_templatemethod_'s execute().
    entries_handler_stub_          = boost::bind<void>(EntriesHandlerStub(), _1);
    enties_ids_handler_stub_       = boost::bind<void>(EntryIDsHandlerStub(), _1, _2);
    entries_count_handler_stub_    = boost::bind<void>(EntriesCountHandlerStub(), _1);
    struct SetEntriesCount {
        mutable size_t* entries_count_;
        SetEntriesCount(size_t* entries_count)
            : entries_count_(entries_count)
        {}
        void operator()(size_t count) const
            { *entries_count_ = count; }
    };
    entries_on_page_count_handler_ = boost::bind<void>(SetEntriesCount(&entries_on_page_), _1);
}

void GetEntryPositionInDataTable::setEntryPageInDataTableFromEntriesList(EntriesRange entries_range,
                                                                         PlaylistEntryID entry_id
                                                                         )
{
    // Note: entry_id is the same as index in entries list. Range entries_range contains full entries list.
    if ( 0 <= entry_id && entry_id < entries_range.size() ) {
       entry_index_in_current_representation_ = entry_id;
    } else {
       // do not set entry_index_in_current_representation_ in case when entry_id is out of range.
    }
}

void  GetEntryPositionInDataTable::setEntryPageInDataTableFromEntryIDs(EntriesIDsRange entries_ids_range, const EntriesListType& /*entries*/,
                                                                       PlaylistEntryID entry_id)
{
    // Note: Range entries_ids_range contains full entries ids list for current representation(concrete filtering and sorting).
    const auto it = std::find(entries_ids_range.begin(), entries_ids_range.end(), entry_id);
    if ( it != entries_ids_range.end() ) {
        entry_index_in_current_representation_ = std::distance(entries_ids_range.begin(), it);
    } else {
        // do not set entry_index_in_current_representation_ in case when entry_id is not in current representation.
    }
}

Rpc::ResponseType GetEntryPositionInDataTable::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& rpc_params = root_request["params"];
    const PlaylistEntryID track_id(rpc_params["track_id"]);

    entries_on_page_ = 0;
    entry_index_in_current_representation_ = -1;

    get_playlist_entries_templatemethod_->execute(rpc_params, 
                                                  // use stubs instead not used callbacks.
                                                  entries_handler_stub_,
                                                  enties_ids_handler_stub_,
                                                  entries_count_handler_stub_,
                                                  entries_count_handler_stub_,

                                                  boost::bind(&GetEntryPositionInDataTable::setEntryPageInDataTableFromEntriesList,
                                                              this, _1, track_id
                                                              ),
                                                  boost::bind(&GetEntryPositionInDataTable::setEntryPageInDataTableFromEntryIDs,
                                                              this, _1, _2, track_id
                                                              ),
                                                  entries_on_page_count_handler_
                                                  );

    //const size_t entries_count_in_representation = current_filtered_entries_count_ != 0 ? current_filtered_entries_count_ : current_total_entries_count_;
    Rpc::Value& rpc_result = root_response["result"];
    if (entries_on_page_ > 0 && entry_index_in_current_representation_ >= 0) {
        rpc_result["page_number"]         = static_cast<size_t>(entry_index_in_current_representation_ / entries_on_page_);
        rpc_result["track_index_on_page"] = static_cast<size_t>(entry_index_in_current_representation_ % entries_on_page_);
    } else {
        rpc_result["page_number"] = -1; ///??? maybe return null here or nothing.
        rpc_result["track_index_on_page"] = -1;
    }
    return RESPONSE_IMMEDIATE;
}

ResponseType GetPlaylistEntriesCount::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() != 1) {
        throw Rpc::Exception("Wrong arguments count. Wait one integer value: playlist_id.", WRONG_ARGUMENT);
    }

    const Playlist& playlist = getPlayListFromRpcParam(aimp_manager_, params["playlist_id"]);

    root_response["result"] = playlist.entries().size(); // max int value overflow is possible, but I doubt that we will work with such huge playlists.
    return RESPONSE_IMMEDIATE;
}

ResponseType GetCover::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() < 2) {
        throw Rpc::Exception("Wrong arguments count. Wait at least 2 int values: track_id, playlist_id.", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);

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

    const std::wstring* cover_uri_cached = isCoverExistsInCoverDirectory(track_desc, cover_width, cover_height);

    if (cover_uri_cached) {
        // picture already exists.
        try {
            root_response["result"]["album_cover_uri"] = StringEncoding::utf16_to_utf8(*cover_uri_cached);
        } catch (StringEncoding::EncodingError&) {
            throw Rpc::Exception("Getting cover failed. Reason: encoding to UTF-8 failed.", ALBUM_COVER_LOAD_FAILED);
        }
        return RESPONSE_IMMEDIATE;
    }


    // save cover to temp directory with unique filename.
    std::wstring cover_uri (cover_directory_);
                    cover_uri += L'/';
                    cover_uri += getTempFileNameForAlbumCover(track_desc, cover_width, cover_height);
                    cover_uri += L".png";
    std::wstring temp_unique_filename (document_root_);
                    temp_unique_filename += L'/';
                    temp_unique_filename += cover_uri;
    try {
        aimp_manager_.savePNGCoverToFile(track_desc, cover_width, cover_height, temp_unique_filename);
        root_response["result"]["album_cover_uri"] = StringEncoding::utf16_to_utf8(cover_uri);
        cover_filenames_[track_desc].push_back(cover_uri);
    } catch (StringEncoding::EncodingError&) {
        throw Rpc::Exception("Getting cover failed. Reason: bad temporarily directory for store covers.", ALBUM_COVER_LOAD_FAILED);
    } catch (std::runtime_error&) {
        throw Rpc::Exception("Getting cover failed. Reason: internal AIMP error.", ALBUM_COVER_LOAD_FAILED);
    }
    return RESPONSE_IMMEDIATE;
}

ResponseType GetPlaylistEntryInfo::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() != 2) {
        throw Rpc::Exception("Wrong arguments count. Wait 2 int values: track_id, playlist_id.", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);

    try {
        const PlaylistEntry& entry = aimp_manager_.getEntry(track_desc);

        using namespace RpcResultUtils;
        using namespace StringEncoding;
        Value& result = root_response["result"];
        result[getStringFieldID(PlaylistEntry::ID)      ] = entry.id();
        result[getStringFieldID(PlaylistEntry::TITLE)   ] = utf16_to_utf8( entry.title() );
        result[getStringFieldID(PlaylistEntry::ARTIST)  ] = utf16_to_utf8( entry.artist() );
        result[getStringFieldID(PlaylistEntry::ALBUM)   ] = utf16_to_utf8( entry.album() );
        result[getStringFieldID(PlaylistEntry::DATE)    ] = utf16_to_utf8( entry.date() );
        result[getStringFieldID(PlaylistEntry::GENRE)   ] = utf16_to_utf8( entry.genre() );
        result[getStringFieldID(PlaylistEntry::BITRATE) ] = static_cast<unsigned int>( entry.bitrate() );
        result[getStringFieldID(PlaylistEntry::DURATION)] = static_cast<unsigned int>( entry.duration() );
        result[getStringFieldID(PlaylistEntry::FILESIZE)] = static_cast<unsigned int>( entry.fileSize() );
        result[getStringFieldID(PlaylistEntry::RATING)  ] = static_cast<unsigned int>( entry.rating() );

    } catch (std::runtime_error&) {
        throw Rpc::Exception("Getting info about track failed. Reason: track not found.", TRACK_NOT_FOUND);
    }
    return RESPONSE_IMMEDIATE;
}

const std::wstring* GetCover::isCoverExistsInCoverDirectory(TrackDescription track_desc, std::size_t width, std::size_t height) const
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
    // search in map of filenames lists.
    const auto iter = cover_filenames_.find(track_desc);
    if (cover_filenames_.end() != iter) {
        const FilenamesList& list = iter->second;
        // search in filenames.
        const auto filename_iter = std::find_if( list.begin(), list.end(), MatchSize(width, height) );
        if (list.end() != filename_iter) {
            // return pointer to found filename.
            return &(*filename_iter);
        }
    }
    return nullptr;
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
    filename << L"cover_" << track_desc.track_id << L"_" << track_desc.playlist_id << "_" << width << "x" << height << "_" << random_file_part_;
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
    case AIMPManager::EVENT_VOLUME:
    case AIMPManager::EVENT_MUTE:
    case AIMPManager::EVENT_SHUFFLE:
    case AIMPManager::EVENT_REPEAT:
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

ResponseType SetTrackRating::execute(const Rpc::Value& root_request, Rpc::Value& /*root_response*/)
{
    const Rpc::Value& params = root_request["params"];
    if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() != 3) {
        throw Rpc::Exception("Wrong arguments count. Wait 3 int values: track ID, playlist ID, rating", WRONG_ARGUMENT);
    }

    const TrackDescription track_desc(params["playlist_id"], params["track_id"]);
    const int rating( Utilities::limit_value<int>(params["rating"], 0, 5) ); // set rating range [0, 5]

    AIMP3Manager* aimp3_manager = dynamic_cast<AIMP3Manager*>(&aimp_manager_);
    if (aimp3_manager) {
        try {
            aimp3_manager->setTrackRating(track_desc, rating);
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error saving rating in "__FUNCTION__". Reason: " << e.what();
            throw Rpc::Exception("Error saving rating.", RATING_SET_FAILED);
        }
    } else {
        // AIMP2 does not support rating set. Save rating in simple text file.
        try {
            const PlaylistEntry& entry = aimp_manager_.getEntry(track_desc);
            std::wofstream file(file_to_save_ratings_, std::ios_base::out | std::ios_base::app);
            file.imbue( std::locale("") ); // set system locale.
            if ( file.good() ) {
                file << entry.filename() << L"; rating:" << rating << L"\n";
                file.close();
            } else {
                throw std::exception("Ratings file can not be opened.");
            }
        } catch (std::exception& e) {
            BOOST_LOG_SEV(logger(), error) << "Error saving rating to text file in "__FUNCTION__". Reason: " << e.what();
            throw Rpc::Exception("Error saving rating to text file.", RATING_SET_FAILED);
        }
    }
    return RESPONSE_IMMEDIATE;
}

} // namespace AimpRpcMethods

#include "aimp/manager2_impl.h"

namespace AimpRpcMethods
{

void EmulationOfWebCtlPlugin::initMethodNamesMap()
{
    boost::assign::insert(method_names_)
        ("get_playlist_list",  get_playlist_list)
        ("get_version_string", get_version_string)
        ("get_version_number", get_version_number)
        ("get_update_time",    get_update_time)
        ("get_playlist_songs", get_playlist_songs)
        ("get_playlist_crc",   get_playlist_crc)
        ("get_player_status",  get_player_status)
        ("get_song_current",   get_song_current)
        ("get_volume",         get_volume)
        ("set_volume",         set_volume)
        ("get_track_position", get_track_position)
        ("set_track_position", set_track_position)
        ("get_track_length",   get_track_length)
        ("get_custom_status",  get_custom_status)
        ("set_custom_status",  set_custom_status)
        ("set_song_play",      set_song_play)
        ("set_song_position",  set_song_position)
        ("set_player_status",  set_player_status)
        ("player_play",        player_play)
        ("player_pause",       player_pause)
        ("player_stop",        player_stop)
        ("player_prevous",     player_prevous)
        ("player_next",        player_next)
        ("playlist_sort",      playlist_sort)
        ("playlist_add_file",  playlist_add_file)
        ("playlist_del_file",  playlist_del_file)
        ("playlist_queue_add", playlist_queue_add)
        ("playlist_queue_remove", playlist_queue_remove)
        ("download_song",      download_song);
}

const EmulationOfWebCtlPlugin::METHOD_ID* EmulationOfWebCtlPlugin::getMethodID(const std::string& method_name) const
{
    const auto it = method_names_.find(method_name);
    return it != method_names_.end() ? &it->second
                                     : nullptr;
}

namespace WebCtl
{

const char * const kVERSION = "2.6.5.0";
const unsigned int kVERSION_INT = 2650;
unsigned int update_time = 10,
             cache_time  = 60;

std::string urldecode(const std::string& url_src)
{
    std::string url_ret = "";
    const size_t len = url_src.length();
    char tmpstr[5] = { '0', 'x', '_', '_', '\0' };

    for (size_t i = 0; i < len; i++) {
        char ch = url_src.at(i);
        if (ch == '%') {
            if ( i+2 >= len ) {
                return "";
            }
            tmpstr[2] = url_src.at( ++i );
            tmpstr[3] = url_src.at( ++i );
            ch = (char)strtol(tmpstr, nullptr, 16);
        }
        url_ret += ch;
    }
    return url_ret;
}

} // namespace WebCtl

void EmulationOfWebCtlPlugin::getPlaylistList(std::ostringstream& out)
{
    AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
    if (aimp2_manager) {
        out << "[";

        boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
        const unsigned int playlist_name_length = 256;
        std::wstring playlist_name;
        const short playlists_count = aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Count();
        for (short i = 0; i < playlists_count; ++i) {
            int playlist_id;
            aimp_playlist_manager->AIMP_PLS_ID_By_Index(i, &playlist_id);
            INT64 duration, size;
            aimp_playlist_manager->AIMP_PLS_GetInfo(playlist_id, &duration, &size);
            playlist_name.resize(playlist_name_length, 0);
            aimp_playlist_manager->AIMP_PLS_GetName( playlist_id, &playlist_name[0], playlist_name.length() );
            playlist_name.resize( wcslen( playlist_name.c_str() ) );
            using namespace Utilities;
            replaceAll(L"\"", 1,
                       L"\\\"", 2,
                       &playlist_name);
            if (i != 0) {
                out << ',';
            }
            out << "{\"id\":" << playlist_id << ",\"duration\":" << duration << ",\"size\":" << size << ",\"name\":\"" << StringEncoding::utf16_to_utf8(playlist_name) << "\"}";
        }
        out << "]";
    } else {
        assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
        throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
    }
}

void EmulationOfWebCtlPlugin::getPlaylistSongs(int playlist_id, bool ignore_cache, bool return_crc, int offset, int size, std::ostringstream& out)
{
    typedef std::string PlaylistCacheKey;
    typedef unsigned int Time;
    typedef int Crc32;
    typedef std::string CachedPlaylist;

    typedef std::map<PlaylistCacheKey,
                     std::pair< Time, std::pair<Crc32, CachedPlaylist> >
                    > PlaylistCache;
    static PlaylistCache playlist_cache;

    bool cacheFound = false;

    std::ostringstream os;
    os << offset << "_" << size << "_" << playlist_id;
    std::string cacheKey = os.str();

    unsigned int tickCount = GetTickCount();

    PlaylistCache::iterator it;

    if (!ignore_cache) {
        // not used, we have one thread. concurencyInstance.EnterReader();
        // check last time
        it = playlist_cache.find(cacheKey);
        if ( (it != playlist_cache.end()) && (tickCount - it->second.first < WebCtl::cache_time * 1000) ) {
            cacheFound = true;
            if (!return_crc) {
                out << it->second.second.second;
            } else {
                out << it->second.second.first;
            }
        }

        // not used, we have one thread. concurencyInstance.LeaveReader();
    }

    if (cacheFound) {
        return;
    }

    // not used, we have one thread. concurencyInstance.EnterWriter();

    AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
    if (!aimp2_manager) {
        assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
        throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
    }

    boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);

    int fileCount = aimp_playlist_manager->AIMP_PLS_GetFilesCount(playlist_id);
    if (size == 0) {
        size = fileCount;
    }
    if (offset < 0) {
        offset = 0;
    }

    out << "{\"status\":";

    if (fileCount < 0) {
        out << "\"ERROR\"";
    } else {
        out << "\"OK\",\"songs\":[";

        const unsigned int entry_title_length = 256;
        std::wstring entry_title;
        AIMP2SDK::AIMP2FileInfo fileInfo = {0};
        fileInfo.cbSizeOf = sizeof(fileInfo);
        for (int i = offset; (i < fileCount) && (i < offset + size); ++i) {
            aimp_playlist_manager->AIMP_PLS_Entry_InfoGet(playlist_id, i, &fileInfo);
            entry_title.resize(entry_title_length, 0);
            aimp_playlist_manager->AIMP_PLS_Entry_GetTitle( playlist_id, i, &entry_title[0], entry_title.length() );
            entry_title.resize( wcslen( entry_title.c_str() ) );
            using namespace Utilities;
            replaceAll(L"\"", 1,
                       L"\\\"", 2,
                       &entry_title);
            if (i != 0) {
                out << ',';
            }
            out << "{\"name\":\"" << StringEncoding::utf16_to_utf8(entry_title) << "\",\"length\":" << fileInfo.nDuration << "}";
        }
        out << "]";
    }
    out << "}";

    // removing old cache
    for (it = playlist_cache.begin(); it != playlist_cache.end(); ) {
        if (tickCount - it->second.first > 60000 * 60) { // delete playlists which where accessed last time hour or more ago
            it = playlist_cache.erase(it);
        } else {
            ++it;
        }
    }

    const std::string out_str = out.str();
    const unsigned int hash = Utilities::crc32(out_str); // CRC32().GetHash(out.str().c_str())

    playlist_cache[cacheKey] = std::pair< int, std::pair<int, std::string> >(tickCount,
                                                                             std::pair<int, std::string>(hash, out_str)
                                                                             );

    if (return_crc) {
        out.str( std::string() );
        out << hash;
    }

    // not used, we have one thread. concurencyInstance.LeaveWriter();
}

void EmulationOfWebCtlPlugin::getPlayerStatus(std::ostringstream& out)
{
    out << "{\"status\": \"OK\", \"RepeatFile\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_REPEAT)
        << ", \"RandomFile\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_SHUFFLE)
        << "}";
}

void EmulationOfWebCtlPlugin::getCurrentSong(std::ostringstream& out)
{
    AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
    if (!aimp2_manager) {
        assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
        throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
    }

    boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
    const int playlist_id  = aimp_playlist_manager->AIMP_PLS_ID_PlayingGet(),
              playing_file = aimp_playlist_manager->AIMP_PLS_ID_PlayingGetTrackIndex(playlist_id);

    std::wstring entry_title(256, 0);
    aimp_playlist_manager->AIMP_PLS_Entry_GetTitle( playlist_id, playing_file, &entry_title[0], entry_title.length() );
    entry_title.resize( wcslen( entry_title.c_str() ) );
    using namespace Utilities;
    replaceAll(L"\"", 1,
               L"\\\"", 2,
               &entry_title);

    out << "{\"status\": \"OK\", \"PlayingList\": "
        << playlist_id
        << ", \"PlayingFile\": "
        << playing_file
        << ", \"PlayingFileName\": \""
        << StringEncoding::utf16_to_utf8(entry_title)
        << "\", \"length\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH)
        << "}";
}

void EmulationOfWebCtlPlugin::calcPlaylistCRC(int playlist_id)
{
    std::ostringstream out;
    getPlaylistSongs(playlist_id, true, true, 0/*params["offset"]*/, 0/*params["size"]*/, out);
}

void EmulationOfWebCtlPlugin::setPlayerStatus(const std::string& statusType, int value)
{
    if (statusType.compare("repeat") == 0) {
        aimp_manager_.setStatus(AIMPManager::STATUS_REPEAT, value);
    } else if (statusType.compare("shuffle") == 0) {
        aimp_manager_.setStatus(AIMPManager::STATUS_SHUFFLE, value);
    }
}

void EmulationOfWebCtlPlugin::sortPlaylist(int playlist_id, const std::string& sortType)
{
    AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
    if (!aimp2_manager) {
        assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
        throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
    }

    boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
    using namespace AIMP2SDK;
    if (sortType.compare("title") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_TITLE);
    } else if (sortType.compare("filename") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_FILENAME);
    } else if (sortType.compare("duration") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_DURATION);
    } else if (sortType.compare("artist") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_ARTIST);
    } else if (sortType.compare("inverse") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_INVERSE);
    } else if (sortType.compare("randomize") == 0) {
        aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_RANDOMIZE);
    }
}

void EmulationOfWebCtlPlugin::addFile(int playlist_id, const std::string& filename_url)
{
    AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
    if (!aimp2_manager) {
        assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
        throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
    }

    AIMP2SDK::IPLSStrings* strings;
    aimp2_manager->aimp2_controller_->AIMP_NewStrings(&strings);
    const std::wstring filename = StringEncoding::utf8_to_utf16( WebCtl::urldecode(filename_url) );
    strings->AddFile(const_cast<PWCHAR>( filename.c_str() ), nullptr);
    aimp2_manager->aimp2_controller_->AIMP_PLS_AddFiles(playlist_id, strings);
}

ResponseType EmulationOfWebCtlPlugin::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    try {
        if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() == 0) {
            throw std::runtime_error("arguments are missing");
        }
        const METHOD_ID* method_id = getMethodID(params["action"]);
        if (method_id == nullptr) {
            std::ostringstream msg;
            msg << "Method " << params["action"] << " not found";
            throw std::runtime_error( msg.str() );
        }

        std::ostringstream out(std::ios::in | std::ios::out | std::ios::binary);

        AIMPPlayer::AIMP2Manager* aimp2_manager = dynamic_cast<AIMPPlayer::AIMP2Manager*>(&aimp_manager_);
        if (!aimp2_manager) {
            assert(!"emulation of web-ctl-plugin on AIMP3 is not implemented yet");
            throw std::runtime_error("not implemented on AIMP3: "__FUNCTION__);
        }

        switch (*method_id) {
        case get_playlist_list:
            getPlaylistList(out);
            break;
        case get_version_string:
            out << WebCtl::kVERSION;
            break;
        case get_version_number:
            out << WebCtl::kVERSION_INT;
            break;
        case get_update_time:
            out << WebCtl::update_time;
            break;
        case get_playlist_songs:
            {
            const int offset = params.isMember("offset") ? params["offset"] : 0;
            const int size = params.isMember("size") ? params["size"] : 0;
            getPlaylistSongs(params["id"], false, false, offset, size, out);
            }
            break;
        case get_playlist_crc:
            getPlaylistSongs(params["id"], false, true, 0/*params["offset"]*/, 0/*params["size"]*/, out);
            break;
        case get_player_status:
            getPlayerStatus(out);
            break;
        case get_song_current:
            getCurrentSong(out);
            break;
        case get_volume:
            out << aimp_manager_.getStatus(AIMPManager::STATUS_VOLUME);
            break;
        case set_volume:
            aimp_manager_.setStatus(AIMPManager::STATUS_VOLUME, params["volume"]);
            break;
        case get_track_position:
            out << "{\"position\":" << aimp_manager_.getStatus(AIMPManager::STATUS_POS) << ",\"length\":" << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH) << "}";
            break;
        case set_track_position:
            aimp_manager_.setStatus(AIMPManager::STATUS_POS, params["position"]);
            break;
        case get_track_length:
            out << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH);
            break;
        case get_custom_status:
            out << aimp2_manager->aimp2_controller_->AIMP_Status_Get( static_cast<int>(params["status"]) ); // use native status getter since web ctl provides access to all statuses.
            break;
        case set_custom_status:
            aimp2_manager->aimp2_controller_->AIMP_Status_Set( static_cast<int>(params["status"]), static_cast<int>(params["value"]) ); // use native status setter since web ctl provides access to all statuses.
            break;
        case set_song_play:
            {
            const TrackDescription track_desc(params["playlist"], params["song"]);
            aimp_manager_.startPlayback(track_desc);
            }
            break;
        case set_song_position:
            {
            const int playlist_id = params["playlist"];
            aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Entry_SetPosition(playlist_id, params["song"], params["position"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case set_player_status:
            setPlayerStatus(params["statusType"], params["value"]);
            break;
        case player_play:
            aimp_manager_.startPlayback();
            break;
        case player_pause:
            aimp_manager_.pausePlayback();
            break;
        case player_stop:
            aimp_manager_.stopPlayback();
            break;
        case player_prevous:
            aimp_manager_.playPreviousTrack();
            break;
        case player_next:
            aimp_manager_.playNextTrack();
            break;
        case playlist_sort:
            {
            const int playlist_id = params["playlist"];
            sortPlaylist(playlist_id, params["sort"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_add_file:
            {
            const int playlist_id = params["playlist"];
            addFile(playlist_id, params["file"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_del_file:
            {
            const int playlist_id = params["playlist"];
            aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Entry_Delete(playlist_id, params["file"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_queue_add:
            {
            const bool insert_at_queue_beginning = false;
            const TrackDescription track_desc(params["playlist"], params["song"]);
            aimp_manager_.enqueueEntryForPlay(track_desc, insert_at_queue_beginning);
            }
            break;
        case playlist_queue_remove:
            {
            const TrackDescription track_desc(params["playlist"], params["song"]);
            aimp_manager_.removeEntryFromPlayQueue(track_desc);
            }
            break;
        case download_song:
            {
            const TrackDescription track_desc(params["playlist"], params["song"]);
            // prepare URI for DownloadTrack::RequestHandler.
            out << "/downloadTrack/playlist_id/" << track_desc.playlist_id 
                << "/track_id/" << track_desc.track_id;
            }
            break;
        default:
            break;
        }

        root_response["result"] = out.str(); // result is simple string.
    } catch (std::runtime_error& e) { // catch all exceptions of aimp manager here.
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__". Reason: " << e.what();

        root_response["result"] = "";
    }
    return RESPONSE_IMMEDIATE;
}

} // namespace AimpRpcMethods
