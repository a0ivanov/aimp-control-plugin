// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "../utils/sqlite_util.h"
#include "common_types.h"

namespace AIMPPlayer
{

//! Represents AIMP playlist.
class Playlist
{
public:
    //! Identificators of playlist fields. 
    enum FIELD_IDs { ID = 0, TITLE, ENTRIES_COUNT, DURATION, SIZE_OF_ALL_ENTRIES_IN_BYTES, CRC32,
                     FIELDS_COUNT // FIELDS_COUNT is special value(not field ID), used to determine fields count.
    };
};

class PlaylistCRC32 {

public:
    PlaylistCRC32(PlaylistID playlist_id, sqlite3* playlist_db);

    crc32_t crc32();

    void reset()
        { crc32_total_ = crc32_properties_ = crc32_entries_ = kCRC32_UNINITIALIZED; }

    void reset_entries()
        { crc32_total_ = crc32_entries_ = kCRC32_UNINITIALIZED; }

    void reset_properties()
        { crc32_total_ = crc32_properties_ = kCRC32_UNINITIALIZED; }

private:
    crc32_t calc_crc32_properties();
    crc32_t calc_crc32_entries();

    PlaylistID playlist_id_;
    sqlite3* playlist_db_;
    crc32_t crc32_total_,
            crc32_properties_,
            crc32_entries_;
};

} // namespace AIMPPlayer

