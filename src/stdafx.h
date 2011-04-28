// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#define NOMINMAX             // Disable max/min macroses.
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers.

#pragma warning (push, 3)

#include <windows.h>

#include <Delayimp.h>    // For error handling & advanced features of delayed DLL load.

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <boost/thread.hpp>

#include <boost/filesystem.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <boost/noncopyable.hpp>

#include <boost/numeric/conversion/cast.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include "boost/timer.hpp"

#include <boost/array.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/assign/ptr_map_inserter.hpp>
#include <boost/range.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std.hpp>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/crc.hpp>

#include <boost/asio.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>

#include <boost/log/common.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/filters.hpp>
#include <boost/log/formatters.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/debug_output_backend.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/exception_handler.hpp>

#include <FreeImagePlus.h>

#include <string>
#include <map>
#include <vector>
#include <list>
#include <cassert>

#include "aimp/aimp2_sdk.h"

#include "config.h"

#pragma warning(pop)
