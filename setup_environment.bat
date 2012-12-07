:: setup paths of all tools used in plugin build process.

rem call "c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
rem call "c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
call "c:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"

:: java runtime is used by yui_compressor and google closure compiler in MINIMIZE_CLIENT_SRC task.
rem set JAVA_HOME=%JAVA_HOME%
set PATH=%PATH%;%JAVA_HOME%\bin

set SUBWCREV_TOOL_DIR=c:\Program Files\TortoiseSVN\bin
set INNO_SETUP_HOME=c:\Program Files (x86)\Inno Setup 5
set PATH=%SUBWCREV_TOOL_DIR%;%INNO_SETUP_HOME%;%PATH%

:: following variables are used for source indexing and storing debug symbols to symbol server.
set PERL_DIR=c:\Perl\ActivePerl\5.16.1.1601
::c:\Program Files\Debugging Tools for Windows (x86)
set DEBUG_TOOL_DIR=c:\Program Files (x86)\Windows Kits\8.0\Debuggers\x86
set SRCSRV_HOME=%DEBUG_TOOL_DIR%\srcsrv
set SVN_CLIENT_DIR=c:\Program Files (x86)\Subversion\bin
set PATH=%DEBUG_TOOL_DIR%;%SRCSRV_HOME%;%PERL_DIR%\bin;%SVN_CLIENT_DIR%\bin;%PATH%
set SYMBOL_STORE=C:\Symbols

:: following variables are used for documentation generation.
set DOXYGEN_PATH=c:\Program Files\doxygen\bin
set PATH=%DOXYGEN_PATH%;%PATH%