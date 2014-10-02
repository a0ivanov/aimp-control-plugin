#pragma once

#include "track_description.h"

namespace AIMPPlayer
{

class IPlaylistEntryRatingManager
{
public:

    /*!
        Sets rating of specified track.

        \param track_desc - track descriptor.
        \param rating - rating value is in range [0-5]. Zero value means rating is not set.
    */
    virtual void trackRating(TrackDescription track_desc, int rating) = 0; // throw std::runtime_error

protected:

    ~IPlaylistEntryRatingManager() {};
};

}