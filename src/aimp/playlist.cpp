// Copyright (c) 2014, Alexey Ivanov

#include "stdafx.h"
#include "aimp/playlist.h"
#include "utils/util.h"

namespace AIMPPlayer {

PlaylistCRC32::PlaylistCRC32(PlaylistID playlist_id, sqlite3* playlist_db)
    :
    playlist_id_(playlist_id),
    playlist_db_(playlist_db),
    crc32_total_(kCRC32_UNINITIALIZED),
    crc32_properties_(kCRC32_UNINITIALIZED),
    crc32_entries_(kCRC32_UNINITIALIZED)
{}

crc32_t PlaylistCRC32::crc32()
{
    if (crc32_properties_ == kCRC32_UNINITIALIZED) {
        crc32_properties_ = calc_crc32_properties();
        crc32_total_ = kCRC32_UNINITIALIZED;
    }

    if (crc32_entries_ == kCRC32_UNINITIALIZED) {
        crc32_entries_ = calc_crc32_entries();
        crc32_total_ = kCRC32_UNINITIALIZED;
    }

    if (crc32_total_ == kCRC32_UNINITIALIZED) {
        // if crc32 was not passed as parameter, calc it here
        const crc32_t crc32_list[] = {
            crc32_properties_,
            crc32_entries_
        };

        crc32_total_ = Utilities::crc32( &crc32_list[0], sizeof(crc32_list) );
    }
    return crc32_total_;
}

crc32_t crc32_text16(const void* text16) 
{
    const wchar_t* text = static_cast<const wchar_t*>(text16);
    return Utilities::crc32( text, wcslen(text) * sizeof(text[0]) );
}

crc32_t PlaylistCRC32::calc_crc32_properties()
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT title, entries_count, duration, size_of_entries FROM Playlists WHERE id=" << playlist_id_;

    sqlite3* db = playlist_db_;
    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            assert(sqlite3_column_count(stmt) == 4);

            const crc32_t members_crc32_list [] = {
                    crc32_text16( sqlite3_column_text16(stmt, 0) ),
                Utilities::crc32( sqlite3_column_int   (stmt, 1) ),
                Utilities::crc32( sqlite3_column_int64 (stmt, 2) ),
                Utilities::crc32( sqlite3_column_int64 (stmt, 3) )
            };

            return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }
    throw std::runtime_error(MakeString() << "Playlist " << playlist_id_ << " is not found in "__FUNCTION__);
}

crc32_t crc32_entry(sqlite3_stmt* stmt);

crc32_t PlaylistCRC32::calc_crc32_entries()
{
    using namespace Utilities;

    std::ostringstream query;
    query << "SELECT "
          << "album, artist, date, filename, genre, title, bitrate, channels_count, duration, filesize, rating, samplerate"
          << " FROM PlaylistsEntries WHERE playlist_id=" << playlist_id_;

    sqlite3* db = playlist_db_;
    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    boost::crc_32_type crc32_calculator;
    for(;;) {
		int rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            const crc32_t crc = crc32_entry(stmt);
            crc32_calculator.process_bytes( &crc, sizeof(crc) );
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    return crc32_calculator.checksum();
}

crc32_t crc32_entry(sqlite3_stmt* stmt)
{
    assert(sqlite3_column_count(stmt) == 12);

    using namespace Utilities;
    const crc32_t members_crc32_list [] = {
            crc32_text16( sqlite3_column_text16(stmt, 0) ),
            crc32_text16( sqlite3_column_text16(stmt, 1) ),
            crc32_text16( sqlite3_column_text16(stmt, 2) ),
            crc32_text16( sqlite3_column_text16(stmt, 3) ),
            crc32_text16( sqlite3_column_text16(stmt, 4) ),
            crc32_text16( sqlite3_column_text16(stmt, 5) ),
                          sqlite3_column_int   (stmt, 6),
                          sqlite3_column_int   (stmt, 7),
                          sqlite3_column_int   (stmt, 8),
        Utilities::crc32( sqlite3_column_int64 (stmt, 9) ),
                          sqlite3_column_int   (stmt,10),
                          sqlite3_column_int   (stmt,11),
    };

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}


} // namespace AIMPPlayer