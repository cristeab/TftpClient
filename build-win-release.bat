set QT_PATH=E:/Qt/5.13.0/msvc2017_64

rmdir /S /Q build

mkdir build
cd build
if %errorlevel% neq 0 exit /b %errorlevel%

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"  x64
if %errorlevel% neq 0 exit /b %errorlevel%

"E:\Program Files\CMake\bin\cmake.exe" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PATH%" -DPACKMSI=on
if %errorlevel% neq 0 exit /b %errorlevel%

nmake
if %errorlevel% neq 0 exit /b %errorlevel%

nmake package
if %errorlevel% neq 0 exit /b %errorlevel%
