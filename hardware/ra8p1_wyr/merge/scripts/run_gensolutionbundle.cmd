@echo off
REM One-time (or after solution.xml edit): embed partition table into merge project.
REM Run from merge\ :
REM   scripts\run_gensolutionbundle.cmd
cd /d "%~dp0.."
if not exist solution.xml (
  python scripts\gen_solution_xml.py
)
del /q output.rasc 2>nul
call rasc_launcher.bat rasc_version.txt -nosplash --launcher.suppressErrors --gensolutionbundle --compiler ARMv6 --devicefamily ra solution.xml 2> "%TEMP%\rasc_stderr.out"
if errorlevel 1 (
  echo [ERROR] gensolutionbundle failed. See %TEMP%\rasc_stderr.out
  exit /b 1
)
echo [OK] solution bundle generated. Rebuild merge in Keil next.
exit /b 0
