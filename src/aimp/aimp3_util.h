#pragma once

#include "stdafx.h"
#include "aimp3_sdk/aimp3_sdk.h"
#include "playlist_entry.h"
#include "utils/util.h"

namespace AIMPPlayer {
namespace AIMP3Util {

/*
    Helper for convinient load entry data from AIMP.
    Prepares AIMP3SDK::TAIMPFileInfo struct.
    Contains necessary string buffers which are always in clear state.
*/
class FileInfoHelper
{
public:

    AIMP3SDK::TAIMPFileInfo& getEmptyFileInfo()
    {
        memset( &info_, 0, sizeof(info_) );
        info_.StructSize = sizeof(info_);
        // clear all buffers content
        WCHAR* field_buffers[] = { album, artist, date, filename, genre, title };
        BOOST_FOREACH(WCHAR* field_buffer, field_buffers) {
            memset( field_buffer, 0, kFIELDBUFFERSIZE * sizeof(field_buffer[0]) );
        }
        // set buffers length
        info_.AlbumBufferSizeInChars = info_.ArtistBufferSizeInChars = info_.DateBufferSizeInChars
        = info_.DateBufferSizeInChars = info_.FileNameBufferSizeInChars = info_.GenreBufferSizeInChars = info_.TitleBufferSizeInChars
        = kFIELDBUFFERSIZE;

        // set buffers
        info_.AlbumBuffer = album;
        info_.ArtistBuffer = artist;
        info_.DateBuffer = date;
        info_.FileNameBuffer = filename;
        info_.GenreBuffer = genre;
        info_.TitleBuffer = title;

        return info_;
    }

    AIMP3SDK::TAIMPFileInfo& getFileInfo()
        { return info_; }

    AIMP3SDK::TAIMPFileInfo& getFileInfoWithCorrectStringLengths()
    {
        // fill string lengths if Aimp does not do this.
        if (info_.AlbumBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.AlbumBufferSizeInChars = std::wcslen(info_.AlbumBuffer);
        }

        if (info_.ArtistBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.ArtistBufferSizeInChars = std::wcslen(info_.ArtistBuffer);
        }

        if (info_.DateBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.DateBufferSizeInChars = std::wcslen(info_.DateBuffer);
        }

        if (info_.FileNameBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.FileNameBufferSizeInChars = std::wcslen(info_.FileNameBuffer);
        }

        if (info_.GenreBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.GenreBufferSizeInChars = std::wcslen(info_.GenreBuffer);
        }

        if (info_.TitleBufferSizeInChars == kFIELDBUFFERSIZE) {
            info_.TitleBufferSizeInChars = std::wcslen(info_.TitleBuffer);
        }

        return info_;
    }

private:

    AIMP3SDK::TAIMPFileInfo info_;
    static const DWORD kFIELDBUFFERSIZE = MAX_PATH;
    WCHAR album[kFIELDBUFFERSIZE + 1];
    WCHAR artist[kFIELDBUFFERSIZE + 1];
    WCHAR date[kFIELDBUFFERSIZE + 1];
    WCHAR filename[kFIELDBUFFERSIZE + 1];
    WCHAR genre[kFIELDBUFFERSIZE + 1];
    WCHAR title[kFIELDBUFFERSIZE + 1];
};

} // namespace AIMP3Util
} // namespace AIMPPlayer
