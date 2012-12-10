@echo off
setlocal ENABLEDELAYEDEXPANSION

call setup_environment.bat

:MAKE_INSTALLER
    call build_installer.bat || goto ERROR_HANDLER
:MAKE_7Z_PACKAGE:
    call make_7z_package.bat || goto ERROR_HANDLER
:MAKE_DOCS:
    set /p PROJECT_VERSION= < %PROJECT_VERSION_FILE% || set PROJECT_VERSION=unknown_version
    call doxygen DoxyfileRpcFunctions || goto ERROR_HANDLER

exit /B


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Failed to make: installer, 7z package, documentation. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%