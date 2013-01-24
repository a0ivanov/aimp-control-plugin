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

std::string getExecutableProductVersion(const TCHAR* pszFilePath) // throws std::runtime_error
{
    using namespace Utilities;

    // got from http://stackoverflow.com/a/940784/1992063

    // get the version info for the file requested
    const DWORD dwSize = GetFileVersionInfoSize( pszFilePath, NULL );
    if ( dwSize == 0 ) {
        throw std::runtime_error( MakeString() << "Error in GetFileVersionInfoSize: " << GetLastError() );
    }

    std::string versionInfoBuf(dwSize, '\0');
    BYTE* pbVersionInfo = reinterpret_cast<BYTE*>( const_cast<std::string::value_type*>(versionInfoBuf.c_str()) );

    if ( !GetFileVersionInfo( pszFilePath, 0, dwSize, pbVersionInfo ) ) {
        throw std::runtime_error( MakeString() << "Error in GetFileVersionInfo: " << GetLastError() );
    }

    VS_FIXEDFILEINFO* pFileInfo = NULL;
    UINT puLenFileInfo = 0;
    if ( !VerQueryValue( pbVersionInfo, TEXT("\\"), (LPVOID*) &pFileInfo, &puLenFileInfo ) ) {
        throw std::runtime_error( MakeString() << "Error in VerQueryValue: " << GetLastError() );
    }

    return MakeString() << (( pFileInfo->dwProductVersionLS >> 24 ) & 0xff)
                        << '.'
                        << (( pFileInfo->dwProductVersionLS >> 16 ) & 0xff)
                        << '.'
                        << (( pFileInfo->dwProductVersionLS >>  8 ) & 0xff)
                        << '.'
                        << (( pFileInfo->dwProductVersionLS >>  0 ) & 0xff)
    ;
}

std::wstring getCurrentExecutablePath() // throws std::runtime_error
{
    HMODULE hm = NULL;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&getCurrentExecutablePath, 
                           &hm)
       )
    {
        using namespace Utilities;
        throw std::runtime_error( MakeString() << "Error in GetModuleHandleEx: " << GetLastError() );
    }

    WCHAR path[MAX_PATH + 1];
    GetModuleFileName(hm, path, MAX_PATH);
    return path;
}

} // namespace Utilities
