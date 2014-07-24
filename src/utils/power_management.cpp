#include "stdafx.h"
#include <PowrProf.h>
#include "utils/util.h"
#include "plugin/logger.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<ControlPlugin::AIMPControlPlugin>(); }
}

namespace PowerManagement
{

bool HibernationEnabled()
{
    #pragma warning (push, 4)
    #pragma warning( disable : 4800 )
    BOOST_LOG_SEV(logger(), debug) << "IsPwrHibernateAllowed(): " << (bool)IsPwrHibernateAllowed();
    BOOST_LOG_SEV(logger(), debug) << "IsPwrShutdownAllowed(): " << (bool)IsPwrShutdownAllowed();
    BOOST_LOG_SEV(logger(), debug) << "IsPwrSuspendAllowed(): " << (bool)IsPwrSuspendAllowed();
    #pragma warning (pop)

    SYSTEM_POWER_CAPABILITIES systemPowerCapabilities = {0};
    if (!GetPwrCapabilities(&systemPowerCapabilities)) {
        DWORD last_error = GetLastError();
        std::string msg = Utilities::MakeString() << "GetPwrCapabilities() failed. Reason: " << last_error;
        BOOST_LOG_SEV(logger(), error) << msg;
        return false;
    }

    #pragma warning (push, 4)
    #pragma warning( disable : 4800 )
    BOOST_LOG_SEV(logger(), debug) << "systemPowerCapabilities.Hiberboot: " << (bool)systemPowerCapabilities.Hiberboot;
    #pragma warning (pop)

    return !!systemPowerCapabilities.Hiberboot;
}

bool SleepEnabled()
{
    return !!IsPwrSuspendAllowed(); // do not sure how to perform it using SYSTEM_POWER_CAPABILITIES.
}

bool ShutdownEnabled()
{
    return !!IsPwrShutdownAllowed(); // do not sure how to perform it using SYSTEM_POWER_CAPABILITIES.
}

bool GetSeShutdownNamePrivelege();

bool SystemShutdown()
{
    if (!GetSeShutdownNamePrivelege()) {
        std::string msg = Utilities::MakeString() << __FUNCTION__": GetSeShutdownNamePrivelege() failed.";
        BOOST_LOG_SEV(logger(), error) << msg;
        return false;
    }

    // Shut down the system and force all applications to close.
    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 
                       SHTDN_REASON_FLAG_USER_DEFINED)
        )
    {
        DWORD last_error = GetLastError();
        std::string msg = Utilities::MakeString() << __FUNCTION__": ExitWindowsEx() failed. Reason: " << last_error;
        BOOST_LOG_SEV(logger(), error) << msg;
        return false; 
    }

    return true;
}

bool SystemSleep(bool hibernate)
{
    if (!GetSeShutdownNamePrivelege()) {
        std::string msg = Utilities::MakeString() << __FUNCTION__": GetSeShutdownNamePrivelege() failed.";
        BOOST_LOG_SEV(logger(), error) << msg;
        return false;
    }

    const bool force = true;
    if (!SetSuspendState(hibernate,
                         force,
                         false)
        )
    {
        DWORD last_error = GetLastError();
        std::string msg = Utilities::MakeString() << __FUNCTION__": SetSuspendState(" << hibernate << ", " << force << ", " << false << ") failed. Reason: " << last_error;
        BOOST_LOG_SEV(logger(), error) << msg;
        return false; 
    }
    return true;
}

bool SystemHibernate()
{
    return SystemSleep(true);
}

bool SystemSleep()
{
    return SystemSleep(false);
}

bool GetSeShutdownNamePrivelege()
{
    HANDLE hToken; 
    TOKEN_PRIVILEGES tkp; 

    // Get a token for this process. 

    if (!OpenProcessToken(GetCurrentProcess(), 
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)
        )
    {
        DWORD last_error = GetLastError();
        std::string msg = Utilities::MakeString() << __FUNCTION__": OpenProcessToken() failed. Reason: " << last_error;
        BOOST_LOG_SEV(logger(), error) << msg;
        return false; 
    }

    // Get the LUID for the shutdown privilege. 

    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
        &tkp.Privileges[0].Luid); 

    tkp.PrivilegeCount = 1;  // one privilege to set    
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 

    // Get the shutdown privilege for this process. 

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
        (PTOKEN_PRIVILEGES)NULL, 0); 

    DWORD last_error = GetLastError();
    if (last_error != ERROR_SUCCESS) {
        std::string msg = Utilities::MakeString() << __FUNCTION__": AdjustTokenPrivileges() failed. Reason: " << last_error;
        BOOST_LOG_SEV(logger(), error) << msg;
        return false;
    }

    return true; 
}

} // namespace PowerManagement