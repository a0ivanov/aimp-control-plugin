@echo off
set EXE_TO_SIGN=%1
set CERT_FILE=%~dp0\certum.pfx
if not exist "%CERT_FILE%" (
    echo No certificate, skip signing of %EXE_TO_SIGN%
    exit /B -1
)

if "%PRIVATE_KEY_PASSWORD%"=="" (
    set /P PRIVATE_KEY_PASSWORD=private key password:
)

set SIGNTOOL=c:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin\signtool.exe
"%SIGNTOOL%" sign /f %CERT_FILE% /p %PRIVATE_KEY_PASSWORD% /t http://timestamp.comodoca.com/authenticode /v %EXE_TO_SIGN%