#pragma once

#include "common_types.h"

namespace AIMPPlayer
{

class IPlaylistUpdateManager
{
public:

    virtual void lockPlaylist(PlaylistID playlist_id) = 0; // throws std::runtime_error
    
    virtual void unlockPlaylist(PlaylistID playlist_id) = 0; // throws std::runtime_error

protected:

    ~IPlaylistUpdateManager() {};
};

}