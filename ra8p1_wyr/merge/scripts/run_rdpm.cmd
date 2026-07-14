@echo off
REM Launch Renesas Device Partition Manager (RDPM) via RASC.
REM Run from merge\ :  scripts\run_rdpm.cmd
cd /d "%~dp0.."
set "RASC=D:\Renesas\RA_new\eclipse\rascc.exe"
if not exist "%RASC%" (
  echo [ERROR] RASC not found: %RASC%
  echo Edit this script if RASC is installed elsewhere.
  exit /b 1
)
set "AXF=Objects\merge.axf"
if not exist "%AXF%" (
  echo [WARN] %AXF% not found — RDPM will still open; build merge in Keil first for best results.
  set "AXF="
)
echo Starting Device Partition Manager...
if defined AXF (
  start "" "%RASC%" -application com.renesas.cdt.ddsc.dpm.ui.dpmapplication configuration.xml "%CD%\%AXF%"
) else (
  start "" "%RASC%" -application com.renesas.cdt.ddsc.dpm.ui.dpmapplication configuration.xml
)
exit /b 0
