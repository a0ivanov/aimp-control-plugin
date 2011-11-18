// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "utils/util.h"
#include <boost/crc.hpp>
#include "plugin/logger.h"

namespace {
using namespace AIMPControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPControlPlugin::AIMP2ControlPlugin>(); }
}

namespace Utilities
{

//! Returns crc32 of buffer[0, length);
crc32_t crc32(const void* buffer, unsigned int length)
{
    static boost::crc_32_type crc32_calculator;
    crc32_calculator.reset();
    crc32_calculator.process_bytes(buffer, length);
    return crc32_calculator.checksum();
}


Profiler::Profiler(const char * const msg)
    :
    msg_(msg)
{}

Profiler::~Profiler()
{
    BOOST_LOG_SEV(logger(), debug) << msg_ << " duration " << timer_.elapsed() << " sec.";
}

} // namespace Utilities
