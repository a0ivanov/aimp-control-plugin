// Copyright (c) 2011, Alexey Ivanov

#ifndef RPC_UTILS_H
#define RPC_UTILS_H

/*!
    \brief rpc_utils.h file contains function for generate RPC answers in unified manner.
*/
#include "aimp/manager.h"
#include "aimp/playlist_entry.h"
#include "utils/string_encoding.h"
#include "rpc/value.h"
#include "plugin/logger.h"
#include <vector>
#include <map>
#include <string>
#include <boost/function.hpp>
#include <boost/foreach.hpp>

namespace AimpRpcMethods
{

//! contains functions which incapsulate mapping AIMP state to string representation in RPC answer.
namespace RpcResultUtils {

void setCurrentPlaybackState(AIMPPlayer::AIMPManager::PLAYBACK_STATE playback_state, Rpc::Value& result);

void setCurrentTrackProgress(size_t current_position, size_t track_length, Rpc::Value& result);

// Sets track progress when it has sense, otherwise does nothing.
void setCurrentTrackProgressIfPossible(const AIMPPlayer::AIMPManager& aimp_manager, Rpc::Value& result);

void setCurrentPlaylist(AIMPPlayer::PlaylistID playlist_id, Rpc::Value& result);

void setCurrentPlaylistEntry(AIMPPlayer::PlaylistEntryID track_id, Rpc::Value& result);

void setCurrentVolume(int volume, Rpc::Value& result);

void setCurrentMuteMode(bool mute_on, Rpc::Value& result);

void setCurrentRepeatMode(bool repeat_on, Rpc::Value& result);

void setCurrentShuffleMode(bool shuffle_on, Rpc::Value& result);

void setCurrentPlayingSourceInfo(const AIMPPlayer::AIMPManager& aimp_manager, Rpc::Value& result);

void setControlPanelInfo(const AIMPPlayer::AIMPManager& aimp_manager, Rpc::Value& result);

void setPlaylistsContentChangeInfo(const AIMPPlayer::AIMPManager& aimp_manager, Rpc::Value& result);

const std::string& getStringFieldID(AIMPPlayer::PlaylistEntry::FIELD_IDs id);

const std::string& getStringFieldID(AIMPPlayer::Playlist::FIELD_IDs id);

} // namespace AimpRpcMethods::RpcResultUtils

//! contains functions for filling requested in PRC methods playlist and entries fields in unified manner.
namespace RpcValueSetHelpers
{

//! Set int field in Rpc value. DWORD, int64, PlaylistID are casted to int.
template <class T>
void setRpcValue(const T& value, Rpc::Value& rpc_value)
{
    rpc_value = static_cast<int>(value);
}

//! Set string field in Rpc struct. String in UTF-16 is converted to string in UTF-8.
template <>
inline void setRpcValue<std::wstring>(const std::wstring& value, Rpc::Value& rpc_value)
{
    rpc_value = StringEncoding::utf16_to_utf8(value);
}


//! Invoker of setRpcValue() function with value which member function of T object returns.
template<class T, class R>
struct AssignerObjectFieldToRpcValue : std::binary_function<const T&, Rpc::Value&, void>
{
    typedef R (T::*FieldGetter)() const;

    AssignerObjectFieldToRpcValue(FieldGetter field_getter)
        :
        field_getter_(field_getter)
    {}

    void operator()(const T& object, Rpc::Value& rpc_value) const
        { setRpcValue( (object.*field_getter_)(), rpc_value ); }

    FieldGetter field_getter_;
};

/*!
    \brief creates AssignerObjectFieldToRpcValue object.
    Deduces template parameters for struct AssignerObjectFieldToRpcValue from pointer to member function.
*/
template<class T, class R>
AssignerObjectFieldToRpcValue<T, R> createSetter(R (T::*field_getter)() const)
{
    return AssignerObjectFieldToRpcValue<T, R>(field_getter);
}

/*!
    \brief Helper class to fill requested in Rpc method playlist and entries fields in unified manner.
    Using:
        1) call initRequiredFieldsHandlersList() to fill requested fields handlers from Rpc value.
            It treats Rpc::Value value as array of strings.
        2) call fillRpcArray() to fill entry or playlist fields in Rpc result value.
            It fills Rpc associative array(keys are requested fields of playlist or entry) by calling data_provider_object member function.
*/
template <class T>
struct HelperFillRpcFields
{
    //! class T is Playlist or PlaylistEntry.
    typedef boost::function<void (const T&, Rpc::Value&)> RpcValueSetter;
    //! map string field ID to function that can assign playlist or track field to RPC value.
    typedef std::map<std::string, RpcValueSetter> RpcValueSetters;
    //! list of pointers to setters. Filled from Rpc method parameter.
    typedef std::vector<typename RpcValueSetters::const_iterator> RpcValueSettersIterators;

    HelperFillRpcFields(const std::string& logger_msg_id)
        :
        logger_msg_id_(logger_msg_id)
    {}

    /*!
        \brief Gets list of required fields of playlist or entry from rpc parameters
               and inits list of correspond field setters.
    */
    void initRequiredFieldsHandlersList(const Rpc::Value& requested_fields) // throws Rpc::Exception
    {
        const size_t requested_fields_count = requested_fields.size();
        setters_required_.clear();
        setters_required_.reserve(requested_fields_count);

        for (size_t field_index = 0; field_index < requested_fields_count; ++field_index) {
            const std::string& field = requested_fields[field_index];
            const auto setter_iterator = setters_.find(field);
            if ( setter_iterator == setters_.end() ) {
                std::ostringstream msg;
                msg << "Wrong argument(" << logger_msg_id_ << "): " << field;
                throw Rpc::Exception(msg.str(), WRONG_ARGUMENT);
            }
            setters_required_.push_back(setter_iterator);
        }
    }

    //! Walks through all required field setters and fills correspond rpc field.
    void fillRpcArrayOfObjects(const T& data_provider_object, Rpc::Value& fields)
    {
        BOOST_FOREACH (const auto& setter, setters_required_) {
            const std::string& field_id = setter->first;
            try {
                setter->second(data_provider_object, fields[field_id]); // invoke functor, that will assign value to fields[field_id].
            } catch (std::exception& e) {
                BOOST_LOG_SEV(logger(), error) << "Error occured while filling AIMP " << logger_msg_id_ << " field " << field_id << ". Reason: " << e.what();
                assert(!"Error occured while filling field in"__FUNCTION__);
                fields[field_id] = std::string();
            }
        }
    }

    //! Walks through all required field setters and fills correspond rpc field.
    void fillRpcArrayOfArrays(const T& data_provider_object, Rpc::Value& fields)
    {
        int index = 0;
        fields.setSize( setters_required_.size() );
        BOOST_FOREACH (const auto& setter, setters_required_) {
            try {
                setter->second(data_provider_object, fields[index]); // invoke functor, that will assign value to fields[field_id].
            } catch (std::exception& e) {
                BOOST_LOG_SEV(logger(), error) << "Error occured while filling AIMP " << logger_msg_id_
                                               << " field " << setter->first << ", field index " << index
                                               << ". Reason: " << e.what();
                assert(!"Error occured while filling field in"__FUNCTION__);
                fields[index] = std::string();
            }
            ++index;
        }
    }

    RpcValueSetters setters_; // list of all supported fields and correspond field getters.
    RpcValueSettersIterators setters_required_; // filled on each initRequiredFieldsList() call from passed Rpc parameters.

private:

    std::string logger_msg_id_; // used to make log messages more informative.
};

} // namespace AimpRpcMethods::RpcValueSetHelpers


//! type of functor that returns string field of specified playlist object. Concrete field to return assinged by boost::bind() in GetPlaylistEntries() ctor.
typedef boost::function<const std::wstring& (const AIMPPlayer::PlaylistEntry& entry)> GetterOfEntryStringField;
typedef std::vector<GetterOfEntryStringField> GettersOfEntryStringField;

/*!
    \brief search specified string occurence in any string field of PlaylistEntry object.
           Search is case independent.
*/
struct FindStringOccurenceInEntryFieldsFunctor
{
    /*!
        \param entry pointer to AIMPPlayer::PlaylistEntry object for search.
        \param search_string string for search.
        \return true if string was found otherwise false.
    */
    bool operator()(const AIMPPlayer::PlaylistEntry& entry, const std::wstring& search_string) const
    {
        BOOST_FOREACH (const auto& getter, field_getters_) {
            const std::wstring& string_to_check = getter(entry);
            boost::iterator_range<std::wstring::const_iterator> result = boost::ifind_first(string_to_check, search_string);
            if ( result.begin() != result.end() ) {
                return true; // if search_string found in one of fields of entry return success flag.
            }
        }

        return false;
    }

    std::vector<GetterOfEntryStringField> field_getters_;
};

} // namespace AimpRpcMethods

#endif // #ifndef RPC_UTILS_H
