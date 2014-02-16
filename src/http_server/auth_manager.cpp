#include "stdafx.h"
#include "http_server/auth_manager.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/util.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Http::Server>(); }
}

namespace Http
{
namespace Authentication
{

using ControlPlugin::PluginLogger::LogManager;
using namespace Utilities;

} // namespace Authentication
} // namespace Http
