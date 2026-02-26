@echo off
setlocal enabledelayedexpansion

REM ===== Settings =====
set "ROOT=%~dp0"
echo Script directory is: %ROOT%
set "VENV=%ROOT%\..\..\.venv"
set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
set "TRISKEL_DIR=%ROOT%"

echo === Ensuring MSVC environment (cl.exe)...

where cl >nul 2>&1
if errorlevel 1 (
    echo MSVC not initialized. Attempting to locate Visual Studio...

    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

    if not exist "%VSWHERE%" (
        echo ERROR: vswhere.exe not found.
        echo Please install Visual Studio 2019 or 2022 with C++ workload.
        exit /b 1
    )

    for /f "usebackq tokens=*" %%i in (`
        "%VSWHERE%" -latest -products * ^
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
        -property installationPath
    `) do set "VSINSTALL=%%i"

    if not defined VSINSTALL (
        echo ERROR: No Visual Studio installation with C++ tools found.
        exit /b 1
    )

    echo Found Visual Studio at:
    echo %VSINSTALL%

    call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64

    where cl >nul 2>&1
    if errorlevel 1 (
        echo ERROR: Failed to initialize MSVC environment.
        exit /b 1
    )
)

echo MSVC environment ready.

echo === Creating venv...
if not exist "%VENV%\Scripts\python.exe" (
  py -3 -m venv "%VENV%"
)
call "%VENV%\Scripts\activate.bat"
python -m pip install --upgrade pip

echo === Installing Python requirements...
if exist "%ROOT%\..\..\requirements.txt" (
  pip install -r "%ROOT%\..\..\requirements.txt"
)
pip install ninja

echo === Checking for Git (git.exe)...
where git >nul 2>&1
if errorlevel 1 (
  echo.
  echo ERROR: git.exe not found on PATH.
  echo Install Git for Windows: https://git-scm.com/download/win
  echo Then reopen your terminal / VS Code so PATH refreshes.
  exit /b 1
)

echo === Installing / bootstrapping vcpkg...
if not exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
  pushd "%USERPROFILE%"
  git clone https://github.com/microsoft/vcpkg
  popd
)

pushd "%VCPKG_ROOT%"
call bootstrap-vcpkg.bat
vcpkg install cairo:x64-windows
popd

pushd "%TRISKEL_DIR%"

echo === Clean build dir...
if exist build rmdir /s /q build

echo === Configuring with CMake + Ninja...
cmake -S . -B build -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DENABLE_CAIRO=ON ^
  -DBUILD_PYTHON_BINDINGS=ON ^
  -DBUILD_JAVA_BINDINGS=OFF ^
  -DENABLE_LLVM=OFF ^
  -DENABLE_IMGUI=OFF ^
  -DPYBIND11_FINDPYTHON=ON ^
  -DPython_EXECUTABLE="%VENV%\Scripts\python.exe" ^
  -DPython_FIND_VIRTUALENV=ONLY ^
  -DPython_FIND_STRATEGY=LOCATION ^
  -DCairo_INCLUDE_DIRS="%VCPKG_ROOT%\installed\x64-windows\include"

echo === Building...
cmake --build build --config Release

echo === Copying Python extension into venv site-packages...

set "SITEPKG=%VENV%\Lib\site-packages"
if not exist "%SITEPKG%" (
  echo Creating: "%SITEPKG%"
  mkdir "%SITEPKG%" >nul 2>&1
  if errorlevel 1 (
    echo ERROR: Failed to create site-packages directory: "%SITEPKG%"
    exit /b 1
  )
)

REM Fail loudly if the build did not produce the .pyd
if not exist "build\bindings\python\pytriskel*.pyd" (
  echo.
  echo ERROR: Built Python module not found:
  echo   build\bindings\python\pytriskel*.pyd
  echo Build may have failed or output path changed.
  exit /b 1
)

xcopy /y /i "build\bindings\python\pytriskel*.pyd" "%SITEPKG%\" >nul
if errorlevel 1 (
  echo ERROR: Failed to copy .pyd into "%SITEPKG%"
  exit /b 1
)

REM Copy DLLs if any exist; don't fail if none are present
if exist "build\bindings\python\*.dll" (
  xcopy /y /i "build\bindings\python\*.dll" "%SITEPKG%\" >nul
  if errorlevel 1 (
    echo ERROR: Failed to copy DLLs into "%SITEPKG%"
    exit /b 1
  )
) else (
  echo (No DLLs found to copy.)
)

popd

echo.
echo DONE.
endlocal