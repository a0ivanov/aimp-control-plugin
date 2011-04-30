@echo off
setlocal ENABLEDELAYEDEXPANSION

call build_plugin.bat || goto ERROR_HANDLER

:: Notice that variable TEMP_BUILD_DIR is set in build_plugin.bat script.

set PROJECT_VERSION_FILE=version.txt

:UPDATE_PROJECT_BUILD_VERSION

    echo Updating project build version...
    SubWCRev .\ version_template.txt %PROJECT_VERSION_FILE% || goto ERROR_HANDLER

:UPDATE_PLUGIN_DLL_ATTRIBUTES
    echo Updating plugin DLL attributes...
    set /p PROJECT_VERSION= < %PROJECT_VERSION_FILE%
    call tools\plugin_dll_attributes_update.bat %TEMP_BUILD_DIR%\aimp_control_plugin.dll %PROJECT_VERSION% || goto ERROR_HANDLER

:SAVE_DEBUG_SYMBOLS
    call tools\save_debug_symbols.bat || goto ERROR_HANDLER

:CREATE_INSTALLER
    echo Creating installer...
    ISCC inno_setup_data\aimp_control_plugin.iss || goto ERROR_HANDLER

echo Build successfull.
exit /B


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Build failed. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%