// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include <iosfwd>
#include "common_types.h"

namespace AIMPPlayer
{

/*!
    Track description.
    Track's absolute coordinates are { playlist ID, track ID in playlist }.
*/
struct TrackDescription
{
    TrackDescription(PlaylistID playlist_id, PlaylistEntryID track_id)
        :
        playlist_id(playlist_id),
        track_id(track_id)
    {}

    PlaylistID playlist_id;
    PlaylistEntryID track_id;
};

std::ostream& operator<<(std::ostream& os, const TrackDescription& track_desc);
bool operator<(const TrackDescription& left, const TrackDescription& right);
bool operator==(const TrackDescription& left, const TrackDescription& right);

} // namespace AIMPPlayer
