#ifndef AIMP3SDK_H
#define AIMP3SDK_H

namespace AIMP3SDK
{

#include <InitGuid.h> // need to force macro DEFINE_GUID to define guid instead declare.

// Add all AIMP SDK functionality to AIMP3SDK namespace.
#include "AIMPSDKAddons.h"
#include "Helpers\AIMPSDKHelpers.h"

} // namespace AIMP3SDK 

#endif // #ifndef AIMP3SDK_H
