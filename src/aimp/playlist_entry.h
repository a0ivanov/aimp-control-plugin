// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_PLAYLIST_ENTRY_H
#define AIMP_PLAYLIST_ENTRY_H

#include "aimp/common_types.h"
#include "utils/util.h"

namespace AIMPPlayer
{

//! Represents playlist entry.
class PlaylistEntry
{
public:
    //! Identificators of playlist entry fields. FIELDS_COUNT is special value(not field ID), used to determine fields count.
    enum FIELD_IDs { ID = 0, TITLE, ARTIST, ALBUM, DATE, GENRE, BITRATE, DURATION, FILENAME, FILESIZE, RATING, INTERNAL_AIMP_ID, ACTIVITY_FLAG, FIELDS_COUNT };

    PlaylistEntry(  const WCHAR* album,    std::size_t album_len,
                    const WCHAR* artist,   std::size_t artist_len,
                    const WCHAR* date,     std::size_t date_len,
                    const WCHAR* filename, std::size_t filename_len,
                    const WCHAR* genre,    std::size_t genre_len,
                    const WCHAR* title,    std::size_t title_len,
                    DWORD bitrate,
                    DWORD channels_count,
                    DWORD duration,
                    INT64 filesize,
                    DWORD rating,
                    DWORD samplerate,
                    DWORD track_id_aimp_internal,
                    DWORD id,
                    crc32_t crc32 = 0
                 );

    PlaylistEntry(PlaylistEntry&& rhs);

    PlaylistEntry& operator=(PlaylistEntry&& rhs);

    //! Returns album.
    const std::wstring& album() const { return album_; }

    //! Returns artist.
    const std::wstring& artist() const { return artist_; }

    //! Returns date(usual only year of track creation).
    const std::wstring& date() const { return date_; }

    //! Returns absolute path to file.
    const std::wstring& filename() const { return filename_; }

    //! Returns genre.
    const std::wstring& genre() const { return genre_; }

    //! Returns title. This field always non empty. If other string fields are empty, this value contains at least file name.
    const std::wstring& title() const { return title_; }

    //! Returns bitrate in bits per seconds.
    DWORD bitrate() const { return bitrate_; }

    //! Returns count of channels.
    DWORD channelsCount() const { return channels_count_; }

    //! Returns duration in milliseconds.
    DWORD duration() const { return duration_; }

    //! Returns file size in bytes.
    INT64 fileSize() const { return filesize_; }

    //! Returns rating in range [0, 5] where 0 means "not set". Currently does not supported by AIMP SDK and always equals zero.
    DWORD rating() const { return rating_; }

    //! Returns sample rate in Hertz(Hz).
    DWORD sampleRate() const { return samplerate_; }

    //! Returns internal AIMP player's ID of track. It seems it is not unique. Not used.
    DWORD trackID() const { return track_id_aimp_internal_; }

    //! Returns external unique PlaylistEntry's ID of track.
    PlaylistEntryID id() const { return id_; }

    //! Returns crc32 of entry. Calculate only if value was not calculated earlier.
    crc32_t crc32() const;

    void swap(PlaylistEntry& rhs);

private:

    //! list of fields of entry in AIMP manner.
    std::wstring album_; //!< album.
    std::wstring artist_; //!< artist.
    std::wstring date_; //!< usual only year of track creation.
    std::wstring filename_; //!< absolute path to file.
    std::wstring genre_; //!< genre.
    std::wstring title_; //!< title. This field always non empty. If other string fields are empty, this value contains at least file name.
    DWORD bitrate_; //!< bitrate in bits per seconds.
    DWORD channels_count_; //!< count of channels.
    DWORD duration_; //!< duration in milliseconds.
    INT64 filesize_; //!< file size in bytes.
    DWORD rating_; //!< rating in range [0, 5] where 0 means "not set". Currently does not supported by AIMP SDK and always equals zero.
    DWORD samplerate_; //!< sample rate in Hertz(Hz).
    DWORD track_id_aimp_internal_; //! internal AIMP player's ID of track. It seems it is not unique. Not used.
    PlaylistEntryID id_; //! external unique PlaylistEntry's ID of track.
    mutable crc32_t crc32_; //! crc32 can be lazy calculated in const crc32() function, so make it mutable.

    PlaylistEntry();
    PlaylistEntry(const PlaylistEntry&);
    PlaylistEntry& operator=(const PlaylistEntry&);
};

} // namespace AIMPPlayer

/*!
    Specialization of Utilities::crc32(T) for PlaylistEntry object.
    Be sure that following assertion is ok:
        AIMP2FileInfo info;
        PlaylistEntry entry(info);
        assert( crc32(entry) == crc32(info) )

*/
template<>
inline crc32_t Utilities::crc32<AIMPPlayer::PlaylistEntry>(const AIMPPlayer::PlaylistEntry& entry)
{
    const crc32_t members_crc32_list [] = {
        crc32( entry.album() ),
        crc32( entry.artist() ),
        crc32( entry.date() ),
        crc32( entry.filename() ),
        crc32( entry.genre() ),
        crc32( entry.title() ),
        entry.bitrate(),
        entry.channelsCount(),
        entry.duration(),
        crc32( entry.fileSize() ),
        entry.rating(),
        entry.sampleRate()
    };

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}

#endif // #ifndef AIMP_PLAYLIST_ENTRY_H
