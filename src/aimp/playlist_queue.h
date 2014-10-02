#pragma once

#include "track_description.h"

namespace AIMPPlayer
{

class IPlaylistQueueManager
{
public:

    virtual void reloadQueuedEntries() = 0; // throws std::runtime_error

    /*!
        Track should be already queued.
    */
    virtual void moveQueueEntry(TrackDescription track_desc, int new_queue_index) = 0; // throws std::runtime_error

    virtual void moveQueueEntry(int old_queue_index, int new_queue_index) = 0; // throws std::runtime_error

protected:

    ~IPlaylistQueueManager() {};
};

}