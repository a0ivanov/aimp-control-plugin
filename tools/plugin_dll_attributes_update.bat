:: Script updates Windows executable file's attributes such a version info and etc.
:: Arguments:
::    <path to executable file>
::    <version>
:: Example: plugin_dll_attributes_update.bat aimp_control_plugin.dll 1.0.0.1

@echo off
setlocal ENABLEDELAYEDEXPANSION

:: save original code page to be able restore it on exit.
for /f "tokens=2 delims=:" %%c in ('chcp') do (
    set ORIGINAL_CODEPAGE=%%c
)

:: change code page to correct saving © symbol in copyright field.
chcp 1252 > nul

set DLL_PATH=%1
set VERSION="%2"
set FILEVERSION=%VERSION%
set FILEDESCR=/s desc "AIMP player plugin. Provides network access to AIMP player."
set COPYRIGHT=/s (c) "Copyright © 2012 Alexey Ivanov"
set PRODINFO=/s product "AIMP Control Plugin"
set PRODVERSION=/pv %VERSION%

:: suppose that verpatch.exe is located in script's directory.
"%~dp0\.\verpatch" /va %DLL_PATH% %FILEVERSION% %FILEDESCR% %COPYRIGHT% %PRODINFO% %PRODVERSION%
set SCRIPT_ERROR_CODE=%ERRORLEVEL%

chcp !ORIGINAL_CODEPAGE! > nul

exit /B !SCRIPT_ERROR_CODE!