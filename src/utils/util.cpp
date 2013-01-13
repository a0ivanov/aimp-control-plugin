// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "utils/util.h"
#include <boost/crc.hpp>
#include "plugin/logger.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<ControlPlugin::AIMPControlPlugin>(); }
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

bool stringStartsWith(const std::string& string, const std::string& search_string)
{
    if ( string.length() >= search_string.length() ) {
        return search_string.find( string.c_str(), 0, search_string.length() ) == 0;
    }
    return false;
}

} // namespace Utilities
