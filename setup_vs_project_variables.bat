:: setup variables which are used in Visual Studio project.
@echo off
call setup_environment.bat > nul

set BOOST_DIR=c:\libraries\boost\boost_1_52_0
set FREEIMAGELIB_DIR=c:\libraries\FreeImage\%FreeImage_VERSION%
::set AIMP_PLUGINS_DIR=%ProgramFiles%\AIMP3\Plugins
set AIMP_PLUGINS_DIR=c:\AIMP\AIMP3.60.1419_beta1\Plugins
set AIMP_CONTROL_SUBPATH=\aimp_control
::for API 3.55- it should be set AIMP_CONTROL_SUBPATH=\