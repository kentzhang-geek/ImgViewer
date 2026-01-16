@echo off
setlocal

REM Setup Visual Studio environment
set VSPATH=D:\Program Files\Microsoft Visual Studio\2022\Community
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Create and enter build directory
if not exist build_ninja mkdir build_ninja
cd build_ninja

REM Configure with Ninja
echo Configuring with Ninja...
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed!
    cd ..
    exit /b %ERRORLEVEL%
)

REM Build
echo Building...
cmake --build .

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    cd ..
    exit /b %ERRORLEVEL%
)

echo Build successful!
cd ..
endlocal
