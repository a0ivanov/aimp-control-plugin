@echo off

:: set paths to external tools which are used in this script.
call setup_environment.bat

:: currently only Release configuration is used. I think Debug config needed for debug purposes so it is much more convinient to use Visual Studio to build debug plugin version.
set CONFIG_NAME=Release
set TEMP_BUILD_DIR=temp_build\%CONFIG_NAME%
set CLIENT_SRC_RELEASE_DIR=%TEMP_BUILD_DIR%\htdocs

setlocal ENABLEDELAYEDEXPANSION

:MINIMIZE_CLIENT_SRC
    :: prepare release version of client side src.
    echo Minimizing of browser scripts...
    set CLOSURE_COMPILER=tools\compiler.jar
    set YUICOMPRESSOR=tools\yuicompressor-2.4.2.jar
    tools\html_optimizer.py -google_closure_compiler !CLOSURE_COMPILER! ^
                            -yuicompressor !YUICOMPRESSOR! ^
                            -output %CLIENT_SRC_RELEASE_DIR% client_src\index.htm || goto ERROR_HANDLER

:COMPILE_PLUGIN
    echo Compiling plugin...
    :: notice that following paths are relative to .\msvc directory where aimp_control_plugin.sln is located.
    set VS_TEMP_DIR=..\%TEMP_BUILD_DIR%\vs\
    call setup_vs_project_variables.bat
    :: variable AIMP_PLUGINS_DIR is used in Visual Studio project. This is the easiest way to make msbuild to put plugin DLL into temp build directory.
    set AIMP_PLUGINS_DIR=..\%TEMP_BUILD_DIR%
    msbuild msvc\aimp_control_plugin.sln /maxcpucount ^
                                         /property:Configuration=%CONFIG_NAME%;IntDir=%VS_TEMP_DIR%;OutDir=%VS_TEMP_DIR% || goto ERROR_HANDLER

echo Build successfull.
exit /B


:ERROR_HANDLER
    set ORIGINAL_ERROR_LEVEL=%ERRORLEVEL%
    echo Build failed. Error code %ORIGINAL_ERROR_LEVEL%
    exit /B %ORIGINAL_ERROR_LEVEL%