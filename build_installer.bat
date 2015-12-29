@echo off
setlocal ENABLEDELAYEDEXPANSION

call build_plugin.bat || goto ERROR_HANDLER

call setup_environment.bat

:: Notice that variable TEMP_BUILD_DIR is set in build_plugin.bat script.

set PLUGIN_DLL=%TEMP_BUILD_DIR%\Control Plugin.dll

:UPDATE_PLUGIN_DLL_ATTRIBUTES
    echo Updating plugin DLL attributes...
    set /p PROJECT_VERSION= < %PROJECT_VERSION_FILE% || set PROJECT_VERSION=unknown_version
    call tools\plugin_dll_attributes_update.bat "%PLUGIN_DLL%" !PROJECT_VERSION! || goto ERROR_HANDLER

:SIGN_PLUGIN_DLL
      echo Skip signing of plugin DLL because cert is not available.
::    echo Signing plugin DLL...
::    call tools\codesign\sign_executable.bat "%PLUGIN_DLL%"
::    if ERRORLEVEL 1 (
::        goto ERROR_HANDLER
::    )
    
:SAVE_DEBUG_SYMBOLS
    call tools\save_debug_symbols.bat || goto ERROR_HANDLER

:CREATE_INSTALLER
    echo Creating installer...
    ISCC inno_setup_data\aimp_control_plugin.iss || goto ERROR_HANDLER

:SIGN_INSTALLER
      echo Skip signing of plugin installer because cert is not available.
::    echo Signing installer...
::    set INSTALLER_FILE=temp_build\Release\distrib\aimp_control_plugin-!PROJECT_VERSION!-setup.exe
::    call tools\codesign\sign_executable.bat %INSTALLER_FILE%
::    if ERRORLEVEL 1 (
::        goto ERROR_HANDLER
::    )
    
echo Build successfull.
exit /B 0


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Build failed. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%