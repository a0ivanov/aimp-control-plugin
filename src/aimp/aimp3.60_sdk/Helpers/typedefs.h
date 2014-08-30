#pragma once

#include <boost/intrusive_ptr.hpp>
#include "../apiPlaylists.h"
#include "../apiObjects.h"

namespace AIMP36SDK
{

typedef boost::intrusive_ptr<IAIMPPlaylist> IAIMPPlaylist_ptr;
typedef boost::intrusive_ptr<IAIMPPlaylistItem> IAIMPPlaylistItem_ptr;
typedef boost::intrusive_ptr<IAIMPString> AIMPString_ptr;

} // namespace AIMP36SDK
