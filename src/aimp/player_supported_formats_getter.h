#pragma once

#include <string>

namespace AIMPPlayer
{

class IPlayerSupportedFormatsGetter
{
public:

    virtual std::wstring supportedTrackExtentions() = 0; // throws std::runtime_error

protected:

    ~IPlayerSupportedFormatsGetter() {};
};

}