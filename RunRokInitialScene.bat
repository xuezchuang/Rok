@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "UE_EDITOR=D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "UPROJECT=%PROJECT_DIR%Rok.uproject"

if not exist "%UE_EDITOR%" (
  echo Missing UnrealEditor.exe: %UE_EDITOR%
  pause
  exit /b 1
)

if not exist "%UPROJECT%" (
  echo Missing project: %UPROJECT%
  pause
  exit /b 1
)

start "" "%UE_EDITOR%" "%UPROJECT%" /Game/Maps/L_RokUnityGameAppCity -log
endlocal
