@echo off
setlocal
set ROOT=%~dp0..
call :build "%ROOT%" || exit /b 1
for /d %%G in ("%ROOT%\games\*") do if exist "%%G\build" call :build "%%G" || exit /b 1
exit /b 0
:build
if not exist "%~1\sdk\tools\premake\premake5.exe" (
  echo ERROR: sdk submodule missing in %~1
  echo Run: git submodule update --init --recursive
  exit /b 1
)
pushd %1
pushd build
..\sdk\tools\premake\premake5.exe gmake || exit /b 1
popd
mingw32-make config=release_x64 || exit /b 1
popd
exit /b 0
