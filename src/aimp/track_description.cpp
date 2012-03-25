// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "track_description.h"
#include <ostream>

namespace AIMPPlayer
{

std::ostream& operator<<(std::ostream& os, const TrackDescription& track_desc)
{
    os << "playlist: " << track_desc.playlist_id << ", track: " << track_desc.track_id;
    return os;
}

bool operator<(const TrackDescription& left, const TrackDescription& right)
{
    return (left.playlist_id < right.playlist_id) ? true
                                                  : (left.playlist_id == right.playlist_id) && (left.track_id < right.track_id) ? true
                                                                                                                                : false;
}

bool operator==(const TrackDescription& left, const TrackDescription& right)
{
    return (left.playlist_id == right.playlist_id) && (left.track_id == right.track_id);
}

} // namespace AIMPPlayer
