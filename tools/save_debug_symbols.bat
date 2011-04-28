@echo off

set PATH_TO_BUILD_PDB=temp_build\Release\vs

:SOURCE_INDEXING
    echo Indexing sources in PDB files...
    :: config-dir set to temp folder to fix error when default config folder path(%APPDATA%\Subversion) contains non ASCII characters.
    set SVNCMD=svn.exe --config-dir temp_build\svn_conf

    :: Note: /Source value is quoted since we need to have full path(otherwise paths to src files will not be found in original PDB)
    :: that can contain spaces.
    call svnindex.cmd /Ini=srcsrv.ini ^
                      "/Source=%CD%\src" ^
                      /Symbols=%PATH_TO_BUILD_PDB% ^
                      /debug ^
                      /DieOnError || goto ERROR_HANDLER

    :: check that our PDB was updated with 
    ::pdbstr -r -p:temp_build\Release\vs\aimp_control_plugin.pdb -s:srcsrv

:SAVE_BUILD_SYMBOLS_IN_SYMSTORE
    echo Saving build symbols in symbol store...
    set /p BUILD_VERSION=<version.txt
    
    symstore add /r /f %PATH_TO_BUILD_PDB%\*.pdb ^
             /s %SYMBOL_STORE% ^
             /v "Build %BUILD_VERSION%" ^
             /t aimp_control_plugin || goto ERROR_HANDLER
 
exit /B


:ERROR_HANDLER
    exit /B %ERRORLEVEL%