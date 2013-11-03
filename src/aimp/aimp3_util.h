#pragma once

#include "stdafx.h"
#include "aimp3_sdk/aimp3_sdk.h"
#include "playlist_entry.h"
#include "utils/util.h"
#include <boost/filesystem.hpp>

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
        resetInfo();
        return info_;
    }

    AIMP3SDK::TAIMPFileInfo& getFileInfo()
        { return info_; }

    AIMP3SDK::TAIMPFileInfo& getFileInfoWithCorrectStringLengthsAndNonEmptyTitle()
    {
        fixStringLengths();
        fixEmptyTitle();
        return info_;
    }

private:

    void resetInfo()
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
        = kFIELDBUFFERSIZEINCHARS;

        // set buffers
        info_.AlbumBuffer = album;
        info_.ArtistBuffer = artist;
        info_.DateBuffer = date;
        info_.FileNameBuffer = filename;
        info_.GenreBuffer = genre;
        info_.TitleBuffer = title;
    }

    void fixStringLengths()
    {
        // fill string lengths if Aimp does not do this.
        if (info_.AlbumBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.AlbumBufferSizeInChars = std::wcslen(info_.AlbumBuffer);
        }

        if (info_.ArtistBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.ArtistBufferSizeInChars = std::wcslen(info_.ArtistBuffer);
        }

        if (info_.DateBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.DateBufferSizeInChars = std::wcslen(info_.DateBuffer);
        }

        if (info_.FileNameBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.FileNameBufferSizeInChars = std::wcslen(info_.FileNameBuffer);
        }

        if (info_.GenreBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.GenreBufferSizeInChars = std::wcslen(info_.GenreBuffer);
        }

        if (info_.TitleBufferSizeInChars == kFIELDBUFFERSIZEINCHARS) {
            info_.TitleBufferSizeInChars = std::wcslen(info_.TitleBuffer);
        }
    }

    // assumption: fixEmptyTitle() method has been already called.
    void fixEmptyTitle()
    {
        if (info_.TitleBufferSizeInChars == 0) {
            boost::filesystem::wpath path(info_.FileNameBuffer);
            path.replace_extension();
            const boost::filesystem::wpath& filename = path.filename();
            info_.TitleBufferSizeInChars = std::min((size_t)kFIELDBUFFERSIZE, std::wcslen(filename.c_str()));
#pragma warning (push, 4)
#pragma warning( disable : 4996 )
            std::wcsncpy(info_.TitleBuffer, filename.c_str(), info_.TitleBufferSizeInChars);
#pragma warning (pop)
        }
    }

    AIMP3SDK::TAIMPFileInfo info_;
    static const DWORD kFIELDBUFFERSIZE = MAX_PATH;
    static const DWORD kFIELDBUFFERSIZEINCHARS = kFIELDBUFFERSIZE - 1;
    WCHAR album[kFIELDBUFFERSIZE];
    WCHAR artist[kFIELDBUFFERSIZE];
    WCHAR date[kFIELDBUFFERSIZE];
    WCHAR filename[kFIELDBUFFERSIZE];
    WCHAR genre[kFIELDBUFFERSIZE];
    WCHAR title[kFIELDBUFFERSIZE];
};

} // namespace AIMP3Util
} // namespace AIMPPlayer
