// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_ENTRIES_SORTER_H
#define AIMP_ENTRIES_SORTER_H

#include "aimp/common_types.h"
#include "aimp/common_types.h"
#include <boost/algorithm/string/predicate.hpp>

namespace AIMPPlayer { namespace EntriesSortUtil {

//! represents sort state of sequence.
enum SEQUENCE_ORDER_STATE { UNORDERED = -1, ASCENDING_ORDER = ASCENDING, DESCENDING_ORDER = DESCENDING };

/*!
    \brief Functor for comparison values of two entry fields.
    Uses std::less<T>() functor.
*/
template <class T>
struct CompareFields : std::binary_function<T, T, bool> {
    bool operator()(const T& left, const T& right) const
        { return std::less<T>()(left, right); }
};

//! Specialization for alphabetical string comparison, case insensitive.
template <>
struct CompareFields<std::wstring> : std::binary_function<std::wstring, std::wstring, bool> {
    bool operator()(const std::wstring& left, const std::wstring& right) const
        { return boost::algorithm::ilexicographical_compare(left, right); }
};

//! Adaptor to be able compare two fields with them IDs instead values.
template <class R, ORDER_DIRECTION order_direction>
struct ComparatorAdaptorFromIDToEntryField : std::binary_function<PlaylistEntryID, PlaylistEntryID, bool>
{
    typedef R (PlaylistEntry::*FieldGetterMemberFunc)() const;

    ComparatorAdaptorFromIDToEntryField(const EntriesListType& entries, FieldGetterMemberFunc getter)
        :
        entries_(entries),
        getter_(getter)
    {}

    bool operator()(PlaylistEntryID id_left, PlaylistEntryID id_right) const
    {
        const R& left_field_value = (entries_[id_left].get()->*getter_)();
        const R& right_field_value = (entries_[id_right].get()->*getter_)();
        if (order_direction == ASCENDING) {
            return compare_less_functor_(left_field_value, right_field_value);
        } else {
            return compare_less_functor_(right_field_value, left_field_value);
        }
    }

    CompareFields<R> compare_less_functor_;
    FieldGetterMemberFunc getter_;
    const EntriesListType& entries_;
};

//! Helper function for creation ComparatorAdaptorFromIDToEntryField object from field getter function.
template<class R, ORDER_DIRECTION order_direction>
ComparatorAdaptorFromIDToEntryField<R, order_direction> createCompareAdaptor( const EntriesListType& entries,
                                                                              R (PlaylistEntry::* getter)() const
                                                                            )
{
    return ComparatorAdaptorFromIDToEntryField<R, order_direction>(entries, getter);
}

/*!
    \brief Interface of entry fields comparators:
    <PRE>
        compare() function compares two entry fields values on basis of them IDs.
            if direction is ASCENDING it uses "less" operator otherwise it uses "greater" operator.
        sortIDsByField() orders sequence of entry IDs with specified direction.
    </PRE>
*/
struct ComparatorPlaylistEntryIDsBase {
    virtual bool compare(PlaylistEntryID id_left, PlaylistEntryID id_right, ORDER_DIRECTION direction) const = 0;
    virtual void sortIDsByField(PlaylistEntryIDList& entry_ids, ORDER_DIRECTION direction) const = 0;
};

//! Class for incapsulate functions that used when ordering entries by concrete field.
template <class R>
struct ComparatorPlaylistEntryIDs : ComparatorPlaylistEntryIDsBase
{
    ComparatorPlaylistEntryIDs(const typename ComparatorAdaptorFromIDToEntryField<R, ASCENDING>& comparator_less,
                               const typename ComparatorAdaptorFromIDToEntryField<R, DESCENDING>& comparator_greater
                               )
        :
        comparator_less_(comparator_less),
        comparator_greater_(comparator_greater)
    {}

    virtual bool compare(PlaylistEntryID id_left, PlaylistEntryID id_right, ORDER_DIRECTION direction) const
    {
        if (direction == DESCENDING) {
            return comparator_greater_(id_left, id_right);
        }
        return comparator_less_(id_left, id_right);
    }

    virtual void sortIDsByField(PlaylistEntryIDList& entry_ids, ORDER_DIRECTION direction) const
    {
        switch (direction)
        {
        case ASCENDING:
            std::sort(entry_ids.begin(), entry_ids.end(), comparator_less_);
            break;
        case DESCENDING:
            std::sort(entry_ids.begin(), entry_ids.end(), comparator_greater_);
            break;
        }
    }

    ComparatorAdaptorFromIDToEntryField<R, ASCENDING> comparator_less_;
    ComparatorAdaptorFromIDToEntryField<R, DESCENDING> comparator_greater_;
};

//! Creates comparator object from field getter function.
template<class R>
std::auto_ptr<ComparatorPlaylistEntryIDsBase> createComparatorPlaylistEntryIDs( const EntriesListType& entries,
                                                                                R (PlaylistEntry::* getter)() const
                                                                               )
{
    return std::auto_ptr<ComparatorPlaylistEntryIDsBase>( new ComparatorPlaylistEntryIDs<R>(
                                                                createCompareAdaptor<R, ASCENDING>(entries, getter),
                                                                createCompareAdaptor<R, DESCENDING>(entries, getter)
                                                                                            )
                                                         );
}

/*!
    \brief Represent entry IDs sequence with order state flag.
    -Sequence order can be in 3 states:
        -# unordered
        -# ascending order
        -# descending order
*/

struct OrderedEntryIDsCache
{
    OrderedEntryIDsCache(std::auto_ptr<ComparatorPlaylistEntryIDsBase> comparator)
        :
        order_state_(UNORDERED),
        comparator_(comparator)
    {}

    void reset()
    {
        sequence_.clear();
        order_state_ = UNORDERED;
    }

    std::vector<PlaylistEntryID> sequence_;
    SEQUENCE_ORDER_STATE order_state_;
    std::auto_ptr<ComparatorPlaylistEntryIDsBase> comparator_;
};


//! Represent ordering(ordering direction and field to order ID).
struct FieldToOrderDescriptor
{
    FieldToOrderDescriptor(ENTRY_FIELDS_ORDERABLE field, ORDER_DIRECTION direction)
        :
        field_(field),
        direction_(direction)
    {}

    ENTRY_FIELDS_ORDERABLE field_;
    ORDER_DIRECTION direction_;
};

typedef std::vector<FieldToOrderDescriptor> FieldToOrderDescriptors;
typedef std::map<ENTRY_FIELDS_ORDERABLE, OrderedEntryIDsCache> OrderedCachesList;

/*!
    \brief Sort entries by specified fields or by multiple fields.
           Sorted entries are presented by them IDs.
*/
class EntriesSorter
{
public:
    EntriesSorter(const EntriesListType& entries);

    const PlaylistEntryIDList& getEntriesSortedByField(FieldToOrderDescriptor order_descriptor);

    const PlaylistEntryIDList& getEntriesSortedByMultipleFields(const FieldToOrderDescriptors& field_to_order_descriptors); // throws std::invalid_argument

    // Reset all ordered caches. Called when entries list is changed.
    // Every time when entry list is changed we need to reset sorter to be sure it does not use old cache.
    void reset();

private:

    OrderedCachesList ordered_caches_;
    const EntriesListType& entries_;
};

} } // namespace AIMPPlayer::EntriesSortUtil

#endif // #ifndef AIMP_ENTRIES_SORTER_H
