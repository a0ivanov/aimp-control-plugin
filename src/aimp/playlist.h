// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_PLAYLIST_H
#define AIMP_PLAYLIST_H

#include "aimp/common_types.h"
#include "aimp/entries_sorter.h"

namespace AIMPPlayer
{

const PlaylistID kCURRENT_PLAYLIST_ID = -1; //!< ID of current active playlist for internal AIMP functions.

//! Represents AIMP playlist.
class Playlist
{
public:
    //! Identificators of playlist fields. FIELDS_COUNT is special value(not field ID), used to determine fields count.
    enum FIELD_IDs { ID = 0, TITLE, ENTRIES_COUNT, DURATION, SIZE_OF_ALL_ENTRIES_IN_BYTES, FIELDS_COUNT };

    Playlist( const CHAR* title,
              DWORD file_count,
              DWORD duration,
              INT64 size_of_all_entries_in_bytes,
              PlaylistID id
            );

    //! Returns map of internal Aimp entry index to plugin entry's ID.
    const EntryIdListInAimpOrder& getAimpOrderedEntriesIDs() const;
    EntryIdListInAimpOrder& getAimpOrderedEntriesIDs();

    //! Returns entries in default order: id ascendence.
    const EntriesListType& getEntries() const;
    void swapEntries(EntriesListType& new_entries);

    //! Returns entries IDs ordered by specified field.
    const PlaylistEntryIDList& getEntriesSortedByField(EntriesSortUtil::FieldToOrderDescriptor order_descriptor) const;

    /*!
        \brief Return entry IDs ordered by multiple fields.
               Sequence of fields for ordering is represented by vector of EntriesSorter::FieldToOrderDescriptor objects.<BR>
        \param unordered_entries - list of PlaylistEntry objects.
        \param field_to_order_descriptors - list of descriptors for multifield sorting.
               Example: we need to order entries by 2 fields, first by album in ascending order, then by title in descending order.<BR>
                        So field_to_order_descriptors will be following: [0]{ALBUM, ASCENDING}, [1]{TITLE, DESCENDING}.
        \return vector of entries IDs ordered as specified.
    */
    const PlaylistEntryIDList& getEntriesSortedByMultipleFields(const EntriesSortUtil::FieldToOrderDescriptors& field_to_order_descriptors) const;

    //! Returns indentificator.
    PlaylistID getID() const { return id_; }

    //! Returns title.
    const std::wstring& getTitle() const { return title_; }

    //! Returns entries count.
    DWORD getEntriesCount() const { return file_count_; }

    //! Returns summary duration of all entries.
    DWORD getDuration() const { return duration_; }

    //! Returns summary size of all entries in bytes.
    INT64 getSizeOfAllEntriesInBytes() const { return size_of_all_entries_in_bytes_; }

private:

    std::wstring title_; //!< title.
    DWORD file_count_; //!< entries count.
    DWORD duration_; //!< summary duration of all entries.
    INT64 size_of_all_entries_in_bytes_; //!< summary size of all entries in bytes.
    PlaylistID id_; //!< playlist indentificator.
    EntriesListType entries_; //!< list of tracks in playlist.
    EntryIdListInAimpOrder entries_id_list_; //!< list of entries IDs in Aimp internal order. Used to determine changes in playlist.

    mutable EntriesSortUtil::EntriesSorter entries_sorter_; //!< util to get sorted entries by multiple fields. Main idea: we always have entries list in original order, but have lists of entries IDs sorted for each field.
};

} // namespace AIMPPlayer

#endif // #ifndef AIMP_PLAYLIST_H
