@echo off
setlocal ENABLEDELAYEDEXPANSION

call build_installer.bat || goto ERROR_HANDLER
call make_7z_package.bat || goto ERROR_HANDLER
call doxygen DoxyfileRpcFunctions || goto ERROR_HANDLER

exit /B


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Failed to make: installer, 7z package, documentation. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%