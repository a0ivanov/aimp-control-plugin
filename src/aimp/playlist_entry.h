// Copyright (c) 2013, Alexey Ivanov

#pragma once

namespace AIMPPlayer
{

class PlaylistEntry
{
public:
    //! Identificators of playlist entry fields.
    enum FIELD_IDs { ID = 0, ALBUM, ARTIST, DATE, FILENAME, GENRE, TITLE, BITRATE, CHANNELS_COUNT, DURATION, FILESIZE, RATING, SAMPLE_RATE,
                     FIELDS_COUNT // FIELDS_COUNT is special value(not field ID), used to determine fields count.
    };
};

} // namespace AIMPPlayer
