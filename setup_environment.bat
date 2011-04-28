:: setup paths of all tools used in plugin build process.

rem call "c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
call "c:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

:: java runtime is used by yui_compressor and google closure compiler in MINIMIZE_CLIENT_SRC task.
set JAVA_HOME=%JAVA_HOME%

set SUBWCREV_TOOL_DIR=c:\Program Files\TortoiseSVN\bin
set INNO_SETUP_HOME=c:\Program Files\Inno Setup 5
set PATH=%SUBWCREV_TOOL_DIR%;%INNO_SETUP_HOME%;%PATH%

:: following variables are used for source indexing and storing debug symbols to symbol server.
set PERL_DIR=c:\perl
set DEBUG_TOOL_DIR=c:\Program Files\Debugging Tools for Windows (x86)
set SRCSRV_HOME=%DEBUG_TOOL_DIR%\srcsrv
set SVN_CLIENT_DIR=c:\Program Files\Subversion\bin
set PATH=%DEBUG_TOOL_DIR%;%SRCSRV_HOME%;%PERL_DIR%\bin;%SVN_CLIENT_DIR%\bin;%PATH%
set SYMBOL_STORE=C:\Symbols