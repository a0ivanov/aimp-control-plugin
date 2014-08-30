#pragma once

#include <InitGuid.h> // need to force macro DEFINE_GUID to define guid instead declare.

namespace AIMP36SDK
{

// Add all AIMP SDK functionality to AIMP360SDK namespace.
#include "apiPlugin.h"
#include "apiPlayer.h"
#include "apiPlaylists.h"
#include "apiObjects.h"
#include "apiFileManager.h"

} // namespace AIMP3SDK 
