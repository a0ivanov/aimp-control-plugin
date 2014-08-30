#include <string.h>
#include "utils/util.h"

namespace AIMP36SDK
{
namespace Support 
{

inline const char* FileinfoPropIdToString(int id)
{
    switch (id)
    {
#define casePropIdToStr(ID) case ID: return ""#ID
    casePropIdToStr(AIMP_FILEINFO_PROPID_CUSTOM);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ALBUM);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ALBUMART);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ALBUMARTIST);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ALBUMGAIN);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ALBUMPEAK);
    casePropIdToStr(AIMP_FILEINFO_PROPID_ARTIST);
    casePropIdToStr(AIMP_FILEINFO_PROPID_BITRATE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_BPM);
    casePropIdToStr(AIMP_FILEINFO_PROPID_CHANNELS);
    casePropIdToStr(AIMP_FILEINFO_PROPID_COMMENT);
    casePropIdToStr(AIMP_FILEINFO_PROPID_COMPOSER);
    casePropIdToStr(AIMP_FILEINFO_PROPID_COPYRIGHT);
    casePropIdToStr(AIMP_FILEINFO_PROPID_CUESHEET);
    casePropIdToStr(AIMP_FILEINFO_PROPID_DATE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_DISKNUMBER);
    casePropIdToStr(AIMP_FILEINFO_PROPID_DISKTOTAL);
    casePropIdToStr(AIMP_FILEINFO_PROPID_DURATION);
    casePropIdToStr(AIMP_FILEINFO_PROPID_FILENAME);
    casePropIdToStr(AIMP_FILEINFO_PROPID_FILESIZE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_GENRE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_LYRICS);
    casePropIdToStr(AIMP_FILEINFO_PROPID_MARK);
    casePropIdToStr(AIMP_FILEINFO_PROPID_PUBLISHER);
    casePropIdToStr(AIMP_FILEINFO_PROPID_SAMPLERATE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_TITLE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_TRACKGAIN);
    casePropIdToStr(AIMP_FILEINFO_PROPID_TRACKNUMBER);
    casePropIdToStr(AIMP_FILEINFO_PROPID_TRACKPEAK);
    casePropIdToStr(AIMP_FILEINFO_PROPID_TRACKTOTAL);
    casePropIdToStr(AIMP_FILEINFO_PROPID_URL);
    casePropIdToStr(AIMP_FILEINFO_PROPID_STAT_ADDINGDATE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_STAT_LASTPLAYDATE);
    casePropIdToStr(AIMP_FILEINFO_PROPID_STAT_MARK);
    casePropIdToStr(AIMP_FILEINFO_PROPID_STAT_PLAYCOUNT);
    casePropIdToStr(AIMP_FILEINFO_PROPID_STAT_RATING);
#undef casePropIdToStr
    }
    throw std::bad_cast( std::string(Utilities::MakeString() << __FUNCTION__": can't convert id " << id << " to string"
                                     ).c_str()
                        );
}

inline const char* PlaylistItemToString(int id)
{
    switch (id)
    {
#define casePropIdToStr(ID) case ID: return ""#ID
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_DISPLAYTEXT);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_FILEINFO);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_FILENAME);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_GROUP);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_INDEX);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_MARK);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_PLAYINGSWITCH);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_PLAYLIST);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_SELECTED);
    casePropIdToStr(AIMP_PLAYLISTITEM_PROPID_PLAYBACKQUEUEINDEX);
#undef casePropIdToStr
    }
    throw std::bad_cast( std::string(Utilities::MakeString() << __FUNCTION__": can't convert id " << id << " to string"
                                     ).c_str()
                        );
}

} // namespace Support
} // namespace AIMP36SDK
