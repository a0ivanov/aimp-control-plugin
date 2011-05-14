// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "entries_sorter.h"
#include "playlist_entry.h"
#include "plugin/logger.h"
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace AIMPPlayer { namespace EntriesSortUtil {

EntriesSorter::EntriesSorter(const EntriesListType& entries)
    :
    entries_(entries)
{
#define MAKE_CACHE_FOR_FIELD(field_name) OrderedEntryIDsCache( createComparatorPlaylistEntryIDs(entries_, &PlaylistEntry::get##field_name) )
    ordered_caches_.insert(std::make_pair(TITLE,    MAKE_CACHE_FOR_FIELD(Title) ) );
    ordered_caches_.insert(std::make_pair(ARTIST,   MAKE_CACHE_FOR_FIELD(Artist) ) );
    ordered_caches_.insert(std::make_pair(ALBUM,    MAKE_CACHE_FOR_FIELD(Album) ) );
    ordered_caches_.insert(std::make_pair(DATE,     MAKE_CACHE_FOR_FIELD(Date) ) );
    ordered_caches_.insert(std::make_pair(GENRE,    MAKE_CACHE_FOR_FIELD(Genre) ) );
    ordered_caches_.insert(std::make_pair(BITRATE,  MAKE_CACHE_FOR_FIELD(Bitrate) ) );
    ordered_caches_.insert(std::make_pair(DURATION, MAKE_CACHE_FOR_FIELD(Duration) ) );
    ordered_caches_.insert(std::make_pair(FILESIZE, MAKE_CACHE_FOR_FIELD(FileSize) ) );
    ordered_caches_.insert(std::make_pair(RATING,   MAKE_CACHE_FOR_FIELD(Rating) ) );
#undef MAKE_CACHE_FOR_FIELD
    assert( FIELDS_ORDERABLE_SIZE == ordered_caches_.size() );
}

bool operator!=(SEQUENCE_ORDER_STATE state, ORDER_DIRECTION direction)
{
    return static_cast<int>(state) != static_cast<int>(direction);
}

/*! If entry_ids list size is differ from entries list size:
        1) copy entry IDs into entry_ids.
        2) reset order state flag to be sure that new sequence will be sorted.
*/
void syncronizeOrderedEntryIDsCacheWithEntriesList(OrderedEntryIDsCache& sorted_cache, const EntriesListType& entries)
{
    PlaylistEntryIDList& entry_ids = sorted_cache.sequence_;
    if ( entry_ids.size() != entries.size() ) {
        entry_ids.clear();
        entry_ids.reserve( entries.size() );

        struct IndexGenerator {
            PlaylistEntryID index_;
            IndexGenerator() : index_(0) {}
            PlaylistEntryID operator()() { return index_++; }
        };

        std::generate_n( std::back_inserter(entry_ids), entries.size(), IndexGenerator() );

        // reset order state flag to be sure that new sequence will be sorted.
        sorted_cache.order_state_ = UNORDERED;
    }
}

const PlaylistEntryIDList& EntriesSorter::getEntriesSortedByField(FieldToOrderDescriptor order_descriptor)
{
    OrderedEntryIDsCache& sorted_cache = ordered_caches_.find(order_descriptor.field_)->second; // search should always be successfull.
    syncronizeOrderedEntryIDsCacheWithEntriesList(sorted_cache, entries_);

    PlaylistEntryIDList& sorted_entry_ids = sorted_cache.sequence_;

    // sort now if it is not sorted right yet.
    if ( order_descriptor.direction_ != sorted_cache.order_state_ ) {
        sorted_cache.comparator_->sortIDsByField(sorted_entry_ids, order_descriptor.direction_);
        sorted_cache.order_state_ = (order_descriptor.direction_ == ASCENDING) ? ASCENDING_ORDER
                                                                               : DESCENDING_ORDER;
    }

    assert( sorted_entry_ids.size() == entries_.size() );
    return sorted_entry_ids;
}

const PlaylistEntryIDList& EntriesSorter::getEntriesSortedByMultipleFields(const FieldToOrderDescriptors& field_to_order_descriptors) // throws std::invalid_argument
{
    if ( field_to_order_descriptors.empty() ) {
        assert(!"Empty field to order descriptors in "__FUNCTION__);
        throw std::invalid_argument("field_to_order_descriptors is empty when it is needed to have at least one field to sort.");
    }

    OrderedEntryIDsCache& sorted_cache = ordered_caches_.find(field_to_order_descriptors[0].field_)->second; // search should always be successfull.
    syncronizeOrderedEntryIDsCacheWithEntriesList(sorted_cache, entries_);

    PlaylistEntryIDList& sorted_entry_ids = sorted_cache.sequence_;

    struct SortByMultipleFieldsFunctor
    {
        SortByMultipleFieldsFunctor(const FieldToOrderDescriptors& field_to_order_descriptors,
                                    const OrderedCachesList& ordered_caches)
            :
            field_to_order_descriptors_(field_to_order_descriptors),
            ordered_caches_(ordered_caches)
        {}

        bool operator()(PlaylistEntryID id_left, PlaylistEntryID id_right) const
        {
            size_t descriptors_size = field_to_order_descriptors_.size();
            for (size_t index = 0; index < descriptors_size; ++index) {
                const FieldToOrderDescriptor& field_desc = field_to_order_descriptors_[index];
                const ComparatorPlaylistEntryIDsBase* comparator = ordered_caches_.find(field_desc.field_)->second.comparator_.get();

                if (index == descriptors_size - 1) { // return result of comparation of last field.
                    return comparator->compare(id_left, id_right, field_desc.direction_);
                } else {
                    if ( comparator->compare(id_left, id_right, field_desc.direction_) ) {
                        return true;
                    }

                    if ( comparator->compare(id_right, id_left, field_desc.direction_) ) {
                        return false;
                    }
                }
            }

            // This point never will be riched since field_to_order_descriptors_ has at least one element.
            return false; // avoid warning C4715: not all control paths return a value.
        }

        const OrderedCachesList& ordered_caches_;
        const FieldToOrderDescriptors& field_to_order_descriptors_;
    };

    std::sort( sorted_entry_ids.begin(), sorted_entry_ids.end(), SortByMultipleFieldsFunctor(field_to_order_descriptors, ordered_caches_) );

    sorted_cache.order_state_ = (field_to_order_descriptors[0].direction_ == ASCENDING) ? ASCENDING_ORDER
                                                                                        : DESCENDING_ORDER;
    assert( sorted_entry_ids.size() == entries_.size() );
    return sorted_entry_ids;
}

void EntriesSorter::reset()
{
    BOOST_FOREACH(OrderedCachesList::value_type& helper_pair, ordered_caches_) {
        OrderedEntryIDsCache& ordered_entry_ids = helper_pair.second;
        ordered_entry_ids.reset();
    }
}

} // namespace EntriesSortUtil
} // namespace AIMPPlayer
