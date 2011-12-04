// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "playlist_entry.h"

namespace AIMPPlayer
{

PlaylistEntry::PlaylistEntry(
                const WCHAR* album,    std::size_t album_len,
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
                crc32_t crc32
                )
    :
    album_(album, album_len),
    artist_(artist, artist_len),
    date_(date, date_len),
    filename_(filename, filename_len),
    genre_(genre, genre_len),
    title_(title, title_len),
    bitrate_(bitrate),
    channels_count_(channels_count),
    duration_(duration),
    filesize_(filesize),
    rating_(rating),
    samplerate_(samplerate),
    track_id_aimp_internal_(track_id_aimp_internal),
    id_(id),
    crc32_(crc32)
{}

PlaylistEntry::PlaylistEntry(PlaylistEntry&& rhs)
    :
    album_   ( std::move(rhs.album_) ),
    artist_  ( std::move(rhs.artist_) ),
    date_    ( std::move(rhs.date_) ),
    filename_( std::move(rhs.filename_) ),
    genre_   ( std::move(rhs.genre_) ),
    title_   ( std::move(rhs.title_) ),
    bitrate_(rhs.bitrate_),
    channels_count_(rhs.channels_count_),
    duration_(rhs.duration_),
    filesize_(rhs.filesize_),
    rating_(rhs.rating_),
    samplerate_(rhs.samplerate_),
    track_id_aimp_internal_(rhs.track_id_aimp_internal_),
    id_(rhs.id_),
    crc32_(rhs.crc32_)
{}

PlaylistEntry& PlaylistEntry::operator=(PlaylistEntry&& rhs)
{
    PlaylistEntry tmp( std::move(rhs) );
    swap(tmp);
    return *this;
}

void PlaylistEntry::swap(PlaylistEntry& rhs)
{
    using std::swap;

    swap(album_, rhs.album_);
    swap(artist_, rhs.artist_);
    swap(date_, rhs.date_);
    swap(filename_, rhs.filename_);
    swap(genre_, rhs.genre_);
    swap(title_, rhs.title_);
    swap(bitrate_, rhs.bitrate_);
    swap(channels_count_, rhs.channels_count_);
    swap(duration_, rhs.duration_);
    swap(filesize_, rhs.filesize_);
    swap(rating_, rhs.rating_);
    swap(samplerate_, rhs.samplerate_);
    swap(track_id_aimp_internal_, rhs.track_id_aimp_internal_);
    swap(id_, rhs.id_);
    swap(crc32_, rhs.crc32_);
}

// Define this method here since specialization of Utilities::crc32() for AIMP2SDK::AIMP2FileInfo should be defined first.
crc32_t PlaylistEntry::crc32() const
{
    if (crc32_ == 0) {
        // if crc32 was not passed as parameter, calc it here
        crc32_ = Utilities::crc32(*this);
    }
    return crc32_;
}

} // namespace AIMPPlayer

