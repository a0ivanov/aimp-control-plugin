// Copyright (c) 2012, Alexey Ivanov

#ifndef AIMP_PLAYLIST_H
#define AIMP_PLAYLIST_H

#include "aimp/common_types.h"
#include "aimp/playlist_entry.h"

namespace AIMPPlayer
{

const PlaylistID kCURRENT_PLAYLIST_ID = -1; //!< ID of current active playlist for internal AIMP functions.

typedef boost::shared_ptr<EntriesListType> EntriesList_ptr;

//! Represents AIMP playlist.
class Playlist
{
public:
    //! Identificators of playlist fields. FIELDS_COUNT is special value(not field ID), used to determine fields count.
    enum FIELD_IDs { ID = 0, TITLE, ENTRIES_COUNT, DURATION, SIZE_OF_ALL_ENTRIES_IN_BYTES, FIELDS_COUNT };

    Playlist();

    Playlist( const WCHAR* title,
              DWORD file_count,
              INT64 duration,
              INT64 size_of_all_entries_in_bytes,
              PlaylistID id
            );

    Playlist(Playlist&& rhs);

    Playlist& operator=(Playlist&& rhs);

    //! Returns entries in default order: id ascendence.
    const EntriesListType& entries() const;
    EntriesListType& entries();

    //! Returns indentificator.
    PlaylistID id() const { return id_; }

    //! Returns title.
    const std::wstring& title() const { return title_; }

    //! Returns entries count.
    DWORD entriesCount() const { return file_count_; }

    //! Returns summary duration of all entries.
    INT64 duration() const { return duration_; }

    //! Returns summary size of all entries in bytes.
    INT64 sizeOfAllEntriesInBytes() const { return size_of_all_entries_in_bytes_; }

    void swap(Playlist& rhs);

    void title(const std::wstring& title) { title_ = title; }

    void entriesCount(DWORD count) { file_count_ = count; }

    void duration(INT64 duration) { duration_ = duration; }

    void sizeOfAllEntriesInBytes(INT64 size) { size_of_all_entries_in_bytes_ = size; }

private:

    std::wstring title_; //!< title.
    DWORD file_count_; //!< entries count.
    INT64 duration_; //!< summary duration of all entries.
    INT64 size_of_all_entries_in_bytes_; //!< summary size of all entries in bytes.
    PlaylistID id_; //!< playlist indentificator.
    EntriesList_ptr entries_; //!< list of tracks in playlist. This must be shared_ptr: entries sorter caches address of entry list.
                              //                                                        But move semantic implies creating of new object.
                              //                                                        So we use separate list object in heap to avoid dangling reference in entries sorter.
    
    Playlist(const Playlist&);
    Playlist& operator=(const Playlist&);
};

} // namespace AIMPPlayer

#endif // #ifndef AIMP_PLAYLIST_H
