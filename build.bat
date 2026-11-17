@echo off
setlocal
cd /d "%~dp0"

set "VCVARS="
for %%P in (
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if exist %%P if not defined VCVARS set "VCVARS=%%~P"

if not defined VCVARS (
  echo ERROR: vcvars64.bat not found. Install Visual Studio 2022 Build Tools with C++ desktop workload.
  exit /b 1
)

call "%VCVARS%" >nul 2>&1
if errorlevel 1 exit /b 1

if not exist "build" mkdir "build"
if not exist "release" mkdir "release"

cl /nologo /LD /O2 /EHsc /MT /W3 /Fo"build\\" /Fe"release\BonDriver_GVMVPVZ_T.dll" "src\BonDriver_GVMVPVZ_T.cpp" /link /IMPLIB:"build\BonDriver_GVMVPVZ_T.lib" bcrypt.lib
if errorlevel 1 exit /b %ERRORLEVEL%

echo Build succeeded: release\BonDriver_GVMVPVZ_T.dll
exit /b 0
