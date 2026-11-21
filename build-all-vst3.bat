@echo off
rem Builds every Melon plugin (VST3, Release) and copies them to builds\windows.
rem Usage: double-click, or run build-all-vst3.bat. Log: build\build-all-vst3.log
setlocal
cd /d "%~dp0"
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set LOG=build\build-all-vst3.log
set PLUGINS=GraphicEQ Chorus Flanger GuitarSynth Dirt Muffs GatesOfDoom Reverberare Delay2290 Rack Comp ProEQ Shifter Tuner SimpleGate MultiDelay Nam Cab

if not exist build mkdir build
echo === configure === > %LOG%
echo Configuring ... (log: %LOG%)
%CMAKE% -B build -A x64 -DFETCHCONTENT_UPDATES_DISCONNECTED=ON >> %LOG% 2>&1 || goto :failed

for %%P in (%PLUGINS%) do (
    echo Building %%P_VST3 ...
    echo === build %%P === >> %LOG%
    %CMAKE% --build build --config Release --target %%P_VST3 >> %LOG% 2>&1 || goto :failed
    robocopy "build\plugins\%%P\%%P_artefacts\Release\VST3" "builds\windows" /E /NJH /NP >> %LOG% 2>&1
    if errorlevel 8 goto :failed
)

echo BUILD OK >> %LOG%
echo.
echo Done: every plugin is in builds\windows
echo To install, copy the .vst3 folders to C:\Program Files\Common Files\VST3\
timeout /t 20
exit /b 0

:failed
echo BUILD FAILED >> %LOG%
echo.
echo Build failed - see %LOG%
pause
exit /b 1
