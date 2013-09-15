@echo off
setlocal ENABLEDELAYEDEXPANSION

call build_plugin.bat || goto ERROR_HANDLER

call setup_environment.bat

:: Notice that variables TEMP_BUILD_DIR and CLIENT_SRC_RELEASE_DIR are set in build_plugin.bat script.

:CREATE_ARCHIVE
    echo Creating archive...
    set /p PROJECT_VERSION= < %PROJECT_VERSION_FILE% || set PROJECT_VERSION=unknown_version
    set FULLPATH_ZIP=%TEMP_BUILD_DIR%\aimp_control_plugin_!PROJECT_VERSION!.7z
    
    :: add htdocs
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\%CLIENT_SRC_RELEASE_DIR% || goto ERROR_HANDLER
    :: add plugin DLL
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\%TEMP_BUILD_DIR%\aimp_control_plugin.dll || goto ERROR_HANDLER         
    :: add default settings.dat
    copy /Y .\inno_setup_data\default_settings.dat .\%TEMP_BUILD_DIR%\settings.dat || goto ERROR_HANDLER 
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\%TEMP_BUILD_DIR%\settings.dat || goto ERROR_HANDLER             
    :: add Readme.dat
    copy /Y .\HowToSetupPluginFrom7zPackage.txt .\%TEMP_BUILD_DIR%\Readme.txt || goto ERROR_HANDLER
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\%TEMP_BUILD_DIR%\Readme.txt || goto ERROR_HANDLER
    :: add FreeImage DLLs
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\3rd_party\FreeImage\%FreeImage_VERSION%\Dist\FreeImage.dll || goto ERROR_HANDLER
    tools\7z a -t7z !FULLPATH_ZIP! ^
             .\3rd_party\FreeImage\%FreeImage_VERSION%\Wrapper\FreeImagePlus\dist\FreeImagePlus.dll || goto ERROR_HANDLER

echo Archive created.
exit /B


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Archive creation failed. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%