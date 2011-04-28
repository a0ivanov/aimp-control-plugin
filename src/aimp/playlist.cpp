// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "aimp/playlist.h"
#include "aimp/common_types.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include <boost/foreach.hpp>

namespace {
using namespace AIMPControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}

namespace AIMPPlayer
{

Playlist::Playlist( const CHAR* title,
                    DWORD file_count,
                    DWORD duration,
                    INT64 size_of_all_entries_in_bytes,
                    PlaylistID id
                  )
    :
    title_( StringEncoding::system_ansi_encoding_to_utf16_safe( std::string(title) ) ),
    file_count_(file_count),
    duration_(duration),
    size_of_all_entries_in_bytes_(size_of_all_entries_in_bytes),
    id_(id),
    entries_(),
    entries_sorter_(entries_)
{
}

const EntriesListType& Playlist::getEntries() const
{
    return entries_;
}

void Playlist::swapEntries(EntriesListType& new_entries)
{
    entries_.swap(new_entries);
    // every time when entry list is changed we need to reset sorter to be sure it does not use old cache.
    entries_sorter_.reset();
}

const EntryIdListInAimpOrder& Playlist::getAimpOrderedEntriesIDs() const
{
    return entries_id_list_;
}

EntryIdListInAimpOrder& Playlist::getAimpOrderedEntriesIDs()
{
    return entries_id_list_;
}

const PlaylistEntryIDList& Playlist::getEntriesSortedByField(EntriesSortUtil::FieldToOrderDescriptor order_descriptor) const
{
    return entries_sorter_.getEntriesSortedByField(order_descriptor);
}

const PlaylistEntryIDList& Playlist::getEntriesSortedByMultipleFields(const EntriesSortUtil::FieldToOrderDescriptors& field_to_order_descriptors) const
{
    return entries_sorter_.getEntriesSortedByMultipleFields(field_to_order_descriptors);
}

} // namespace AIMPPlayer
