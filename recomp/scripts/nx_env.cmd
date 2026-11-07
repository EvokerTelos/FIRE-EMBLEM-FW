@echo off
rem Sets up the build environment: MSVC x64 (vcvars64), LLVM clang-cl 23, CMake, Ninja, sccache.
rem Usage from cmd:  scripts\nx_env.cmd cmake --build build\x
rem                  scripts\nx_env.cmd            (no arguments: just prints the tool versions)
rem a stray quote in PATH (e.g. C:\Program Files\dotnet") breaks every later line that expands it: drop all quotes first
set "PATH=%PATH:"=%"
set "_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "_VSDIR="
if exist "%_VSWHERE%" for /f "usebackq delims=" %%i in (`"%_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "_VSDIR=%%i"
if not defined _VSDIR set "_VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
call "%_VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links;%~dp0..\..\tools\.venv\Scripts;%PATH%"
if "%~1"=="" (
  clang-cl --version & cmake --version & ninja --version & sccache --version
  exit /b 0
)
%*
