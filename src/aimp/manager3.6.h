// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "manager.h"
#include "aimp3.60_sdk/aimp3_60_sdk.h"
#include "utils/util.h"

namespace AIMPPlayer
{

/*!
    \brief Extends AIMPManager with new functionality introduced in AIMP 3.60.
*/
class AIMPManager36 : public AIMPManager
{
public:

    AIMPManager36(boost::intrusive_ptr<AIMP36SDK::IAIMPCore> aimp36_core); // throws std::runtime_error

    virtual ~AIMPManager36();
};

} // namespace AIMPPlayer
