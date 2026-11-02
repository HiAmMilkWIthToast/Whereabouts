@echo off
rem Builds Cairn with the Qt/MinGW install on this machine.
rem
rem   build.bat            build build\cairn.exe
rem   build.bat deploy     also stage a runnable copy in release\
setlocal

set "QT_DIR=C:/Qt/6.12.0/mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
set "CMAKE_DIR=C:\Qt\Tools\CMake_64"
set "NINJA_DIR=C:\Qt\Tools\Ninja"
set "BUILD=build"
set "TARGET=%~1"

set "PATH=%CMAKE_DIR%\bin;%NINJA_DIR%;%MINGW_DIR%\bin;%BUILD%;%QT_DIR%\bin;%PATH%"

if not exist %BUILD%\CMakeCache.txt (
  echo [1/2] Configuring...
  "%CMAKE_DIR%\bin\cmake.exe" -S . -B %BUILD% -G Ninja ^
    -DCMAKE_PREFIX_PATH=%QT_DIR% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_COMPILER=%MINGW_DIR%\bin\g++.exe
  if errorlevel 1 exit /b 1
)

echo [2/2] Building...
"%CMAKE_DIR%\bin\cmake.exe" --build %BUILD%
if errorlevel 1 exit /b 1

rem Stage the Qt runtime beside the exe so it runs without the Qt install.
copy /y "%QT_DIR%\bin\Qt6Core.dll"    %BUILD%\ >nul
copy /y "%QT_DIR%\bin\Qt6Gui.dll"     %BUILD%\ >nul
copy /y "%QT_DIR%\bin\Qt6Widgets.dll" %BUILD%\ >nul
copy /y "%QT_DIR%\bin\Qt6Svg.dll"     %BUILD%\ >nul
if not exist %BUILD%\platforms mkdir %BUILD%\platforms
copy /y "%QT_DIR%\plugins\platforms\qwindows.dll" %BUILD%\platforms\ >nul
copy /y "%MINGW_DIR%\bin\libgcc_s_seh-1.dll"     %BUILD%\ >nul
copy /y "%MINGW_DIR%\bin\libstdc++-6.dll"        %BUILD%\ >nul
copy /y "%MINGW_DIR%\bin\libwinpthread-1.dll"    %BUILD%\ >nul

if /i "%TARGET%"=="deploy" goto deploy
if not "%TARGET%"=="" goto usage

echo.
echo Build OK: %BUILD%\cairn.exe
exit /b 0

:deploy
rem release\ is the tracked, runnable copy: the exe plus exactly the Qt and
rem MinGW DLLs it needs, and the platform plugin. It is rebuilt from scratch each
rem time so a stale DLL can never be left behind.
if exist release rmdir /s /q release
mkdir release
copy /y %BUILD%\cairn.exe release\ >nul
"%QT_DIR%\bin\windeployqt.exe" --release --compiler-runtime ^
  --no-translations --no-opengl-sw --no-system-d3d-compiler ^
  --skip-plugin-types imageformats,iconengines,generic,styles,tls,networkinformation ^
  release\cairn.exe
if errorlevel 1 exit /b 1
echo Staged: release\cairn.exe
exit /b 0

:usage
echo Unknown option "%TARGET%". Use: build.bat [deploy]
exit /b 1
