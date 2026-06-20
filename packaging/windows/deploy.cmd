@echo off
rem ---------------------------------------------------------------------------
rem Double-click launcher for deploy.ps1 (the Winnow Windows deploy menu).
rem
rem Runs the PowerShell script next to it with ExecutionPolicy Bypass so it
rem works from Explorer without changing the machine policy. Self-locating:
rem %~dp0 is this file's own directory, so it works unchanged after a git clone.
rem
rem For signing/build you still want a Developer PowerShell for VS 2022 (x64) on
rem PATH; launching from there (or a VS x64 Native Tools prompt) ensures cmake,
rem the MSVC toolchain, and signtool resolve correctly.
rem ---------------------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1"
pause
