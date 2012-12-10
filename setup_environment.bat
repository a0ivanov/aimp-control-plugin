:: setup paths of all tools used in plugin build process.

:: Avoid MSBuild's error MSB8008: Specified platform toolset (v110) is not installed or invalid.
set VisualStudioVersion=11.0
call "%ProgramFiles(x86)%\Microsoft Visual Studio %VisualStudioVersion%\VC\bin\vcvars32.bat"

:: java runtime is used by yui_compressor and google closure compiler in MINIMIZE_CLIENT_SRC task.
rem set JAVA_HOME=%JAVA_HOME%
set PATH=%PATH%;%JAVA_HOME%\bin

set SUBWCREV_TOOL_DIR=%ProgramFiles%\TortoiseSVN\bin
set INNO_SETUP_HOME=%ProgramFiles(x86)%\Inno Setup 5
set PATH=%SUBWCREV_TOOL_DIR%;%INNO_SETUP_HOME%;%PATH%

:: following variables are used for source indexing and storing debug symbols to symbol server.
set PERL_DIR=c:\Perl\ActivePerl\5.16.1.1601
:: Before windows 8 it was here: %ProgramFiles%\Debugging Tools for Windows (x86)
set DEBUG_TOOL_DIR=%ProgramFiles(x86)%\Windows Kits\8.0\Debuggers\x86
set SRCSRV_HOME=%DEBUG_TOOL_DIR%\srcsrv
set SVN_CLIENT_DIR=%ProgramFiles(x86)%\Subversion\bin
set PATH=%DEBUG_TOOL_DIR%;%SRCSRV_HOME%;%PERL_DIR%\bin;%SVN_CLIENT_DIR%\bin;%PATH%
set SYMBOL_STORE=C:\Symbols

:: following variables are used for documentation generation.
set DOXYGEN_PATH=%ProgramFiles%\doxygen\bin
set PATH=%DOXYGEN_PATH%;%PATH%