@echo off
call setup_vs_project_variables.bat > nul
:: Visual Studio version is chosen by following line in sln file:
:: # Visual C++ Express 2012
rem start msvc\aimp_control_plugin.sln

:: This starts Visual Studio 2012 even if aimp_control_plugin.sln points to Visual C++ Express 2010
::start "%ProgramFiles(x86)%\Microsoft Visual Studio 11.0\Common7\IDE\WDExpress" msvc\aimp_control_plugin.sln

start msvc\aimp_control_plugin.sln