@echo off
call setup_vs_project_variables.bat > nul
:: Visual Studio version is chosen by following line in sln file:
:: # Visual C++ Express 2012
rem start msvc\aimp_control_plugin.sln

:: This starts Visual Studio 2013 even if aimp_control_plugin.sln points to Visual C++ Express 2010
"%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\IDE\devenv" "%~dp0\msvc\aimp_control_plugin.sln"

::start msvc\aimp_control_plugin.sln