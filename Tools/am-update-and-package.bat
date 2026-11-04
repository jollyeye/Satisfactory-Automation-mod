@echo off
rem Automated Machines: one-click update. Copy this file once to D:\SFMods\ and run it from there.
rem It only gets the latest code; the build steps live in the repo (Tools\build-and-package.bat),
rem so they can change without copying this file again.
set MODDIR=D:\SFMods\SML\Mods\AutomatedMachines
set LOG=D:\SFMods\am-update-log.txt
set GIT="%ProgramFiles%\Git\cmd\git.exe"

echo [1/4] Getting the latest Automated Machines code...
echo == git pull == > "%LOG%"
%GIT% -C "%MODDIR%" pull >> "%LOG%" 2>&1
if errorlevel 1 goto failed

call "%MODDIR%\Tools\build-and-package.bat"
exit /b %errorlevel%

:failed
echo FAILED >> "%LOG%"
echo.
echo ***** FAILED: could not get the latest code. *****
echo Tell Claude, it can read D:\SFMods\am-update-log.txt
pause
exit /b 1
