@echo off
rem Automated Machines: build the editor, make the assets, package into the game.
rem Called by D:\SFMods\am-update-and-package.bat after it got the latest code.
rem Edit the paths below if your folders differ.
set PROJECT=D:\SFMods\SML\FactoryGame.uproject
set MODDIR=D:\SFMods\SML\Mods\AutomatedMachines
set GAME=D:\games\Satisfactory
set LOG=D:\SFMods\am-update-log.txt
set ENGINE=D:\UE-CSS
if exist "D:\UE-CSS\Unreal Engine - CSS\Engine" set ENGINE=D:\UE-CSS\Unreal Engine - CSS

echo [2/4] Building the editor...
echo == build == >> "%LOG%"
call "%ENGINE%\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -Project="%PROJECT%" -WaitMutex >> "%LOG%" 2>&1
if errorlevel 1 goto failed

echo [3/4] Making the mod's assets (icons, marker)...
echo == assets == >> "%LOG%"
"%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -run=pythonscript -script="%MODDIR%\Tools\create_assets.py" -unattended -nop4 -nosplash -stdout > D:\SFMods\am-assets-editor-log.txt 2>&1

echo [4/4] Packaging Automated Machines into the game...
echo == package == >> "%LOG%"
call "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="%PROJECT%" PackagePlugin -project="%PROJECT%" -clientconfig=Shipping -serverconfig=Shipping -utf8output -build -platform=Win64 -nocompileeditor -installed -Target=FactoryGameEGS -CopyToGameDirectory_Windows="%GAME%" -DLCName=AutomatedMachines >> "%LOG%" 2>&1
if errorlevel 1 goto failed

echo ALL DONE >> "%LOG%"
echo ALL DONE - start the game and test. Tell Claude.
pause
exit /b 0

:failed
echo FAILED >> "%LOG%"
echo.
echo ***** FAILED: the game still has the OLD version of Automated Machines. *****
echo Tell Claude, it can read D:\SFMods\am-update-log.txt
pause
exit /b 1
