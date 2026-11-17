@echo off
rem Builds one Melon plugin (VST3, Release) and copies it to builds\windows.
rem Usage: build-vst3.bat [Target]   (default: GuitarSynth). Log: build\build-vst3.log
setlocal
cd /d "%~dp0"
set TARGET=%~1
if "%TARGET%"=="" set TARGET=GuitarSynth
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set LOG=build\build-vst3.log

echo Building %TARGET%_VST3 ... (log: %LOG%)
echo === configure === > %LOG%
%CMAKE% -B build -A x64 -DFETCHCONTENT_UPDATES_DISCONNECTED=ON >> %LOG% 2>&1 || goto :failed
echo === build === >> %LOG%
%CMAKE% --build build --config Release --target %TARGET%_VST3 >> %LOG% 2>&1 || goto :failed

set SRC=build\plugins\%TARGET%\%TARGET%_artefacts\Release\VST3
echo === copy === >> %LOG%
robocopy "%SRC%" "builds\windows" /E /NJH /NP >> %LOG% 2>&1
if errorlevel 8 goto :failed
echo BUILD OK >> %LOG%
echo.
echo Done: the plugin is in builds\windows
timeout /t 15
exit /b 0

:failed
echo BUILD FAILED >> %LOG%
echo.
echo Build failed - see %LOG%
pause
exit /b 1
