:: setup variables which are used in Visual Studio project.
@echo off
call setup_environment.bat > nul

set BOOST_DIR=c:\libraries\boost\boost_1_61_0
set FREEIMAGELIB_DIR=%~dp0\3rd_party\FreeImage\%FreeImage_VERSION%
set AIMP_PLUGINS_DIR=c:\AIMP\AIMP4.02.1725\Plugins
::for API 3.55- it should be
::  set AIMP_CONTROL_SUBPATH=\
set AIMP_CONTROL_SUBPATH=\Control Plugin
