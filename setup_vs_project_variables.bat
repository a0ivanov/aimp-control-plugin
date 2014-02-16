:: setup variables which are used in Visual Studio project.
@echo off
call setup_environment.bat > nul

set BOOST_DIR=c:\libraries\boost\boost_1_52_0
set FREEIMAGELIB_DIR=c:\libraries\FreeImage\%FreeImage_VERSION%
::set AIMP_PLUGINS_DIR=%ProgramFiles%\AIMP3\Plugins
set AIMP_PLUGINS_DIR=c:\AIMP\AIMP3.55.1338\Plugins