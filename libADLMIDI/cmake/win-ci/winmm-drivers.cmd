rem =============== BUILDING THE WINMM DRIVERS, USING DIFFERENT MINGW TOOLCHAINS ===============

set MINGW_VANILLA_URL=http://wohlsoft.ru/docs/Software/MinGW/MinGW-6-3-x86-dw2.zip
set MINGW_W64_32_URL=http://wohlsoft.ru/docs/Software/MinGW/i686-8.1.0-release-posix-dwarf-rt_v6-rev0.7z
set MINGW_W64_64_URL=http://wohlsoft.ru/docs/Software/MinGW/x86_64-8.1.0-release-posix-seh-rt_v6-rev0.7z
set NINJA_URL=http://wohlsoft.ru/docs/Software/Ninja-Build/ninja-win.zip

md C:\mingw-temp
md C:\mingw-temp\bin

echo Downloading %NINJA_URL%...
"%WGET_BIN%" --quiet %NINJA_URL% -O C:\mingw-temp\ninja.zip
if %errorlevel% neq 0 exit /b %errorlevel%
"%SEVENZIP%" x C:\mingw-temp\ninja.zip -oC:\mingw-temp\bin
if %errorlevel% neq 0 exit /b %errorlevel%

md C:\mingw-temp\MinGW32
md C:\mingw-temp\MinGW-w64

echo Downloading %MINGW_VANILLA_URL%...
"%WGET_BIN%" --quiet %MINGW_VANILLA_URL% -O C:\mingw-temp\mingw-vanilla.zip
if %errorlevel% neq 0 exit /b %errorlevel%
"%SEVENZIP%" x C:\mingw-temp\mingw-vanilla.zip -oC:\mingw-temp\MinGW32
if %errorlevel% neq 0 exit /b %errorlevel%

echo Downloading %MINGW_W64_32_URL%...
"%WGET_BIN%" --quiet %MINGW_W64_32_URL% -O C:\mingw-temp\mingw-w64-32.7z
if %errorlevel% neq 0 exit /b %errorlevel%
"%SEVENZIP%" x C:\mingw-temp\mingw-w64-32.7z -oC:\mingw-temp\MinGW-w64
if %errorlevel% neq 0 exit /b %errorlevel%

echo Downloading %MINGW_W64_64_URL%...
"%WGET_BIN%" --quiet %MINGW_W64_64_URL% -O C:\mingw-temp\mingw-w64-64.7z
if %errorlevel% neq 0 exit /b %errorlevel%
"%SEVENZIP%" x C:\mingw-temp\mingw-w64-64.7z -oC:\mingw-temp\MinGW-w64
if %errorlevel% neq 0 exit /b %errorlevel%

set PUREPATH=C:\mingw-temp\bin;%PATH:C:\Program Files\Git\usr\bin;=%

set FILES_LIST=adlmidiconfigtool.exe adlmididrv.dll libadlconfig.cpl drvsetup.exe drvtest.exe install.bat uninstall.bat

rem ============= BUILD pre-WinXP 32-bit driver =============

md build-drv-prexp
cd build-drv-prexp

set CMAKEPREFIXPATH=C:/mingw-temp/MinGW32
set TOOLCHAIN_BIN=C:\mingw-temp\MinGW32\bin
set PATH=%TOOLCHAIN_BIN%;%PUREPATH%

cmake -G "%GENERATOR%"^
 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%^
 -DCMAKE_PREFIX_PATH=%CMAKEPREFIXPATH%^
 -DCMAKE_INSTALL_PREFIX=libADLMIDI^
 -DWITH_OLD_UTILS=OFF ^
 -DWITH_GENADLDATA=OFF ^
 -DlibADLMIDI_STATIC=ON ^
 -DlibADLMIDI_SHARED=OFF ^
 -DWITH_MIDIPLAY=OFF ^
 -DWITH_VLC_PLUGIN=OFF ^
 -DWITH_WINMMDRV=ON ^
 -DWITH_WINMMDRV_PTHREADS=ON ^
 -DWITH_WINMMDRV_MINGWEX=ON ^
 ..
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build . --config %BUILD_TYPE% -- -j 2
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..


rem ============= BUILD WinXP+ 32-bit driver =============

md build-drv-32
cd build-drv-32

set CMAKEPREFIXPATH=C:/mingw-temp/MinGW-w64/mingw32
set TOOLCHAIN_BIN=C:\mingw-temp\MinGW-w64\mingw32\bin
set PATH=%TOOLCHAIN_BIN%;%PUREPATH%

cmake -G "%GENERATOR%"^
 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%^
 -DCMAKE_PREFIX_PATH=%CMAKEPREFIXPATH%^
 -DCMAKE_INSTALL_PREFIX=libADLMIDI^
 -DWITH_OLD_UTILS=OFF ^
 -DWITH_GENADLDATA=OFF ^
 -DlibADLMIDI_STATIC=ON ^
 -DlibADLMIDI_SHARED=OFF ^
 -DWITH_MIDIPLAY=OFF ^
 -DWITH_VLC_PLUGIN=OFF ^
 -DWITH_WINMMDRV=ON ^
 -DWITH_WINMMDRV_PTHREADS=ON ^
 -DWITH_WINMMDRV_MINGWEX=OFF ^
 ..
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build . --config %BUILD_TYPE% -- -j 2
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..


rem ============= BUILD WinXP+ 64-bit driver =============

md build-drv-64
cd build-drv-64

set CMAKEPREFIXPATH=C:/mingw-temp/MinGW-w64/mingw64
set TOOLCHAIN_BIN=C:\mingw-temp\MinGW-w64\mingw64\bin
set PATH=%TOOLCHAIN_BIN%;%PUREPATH%

cmake -G "%GENERATOR%"^
 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%^
 -DCMAKE_PREFIX_PATH=%CMAKEPREFIXPATH%^
 -DCMAKE_INSTALL_PREFIX=libADLMIDI^
 -DWITH_OLD_UTILS=OFF ^
 -DWITH_GENADLDATA=OFF ^
 -DlibADLMIDI_STATIC=ON ^
 -DlibADLMIDI_SHARED=OFF ^
 -DWITH_MIDIPLAY=OFF ^
 -DWITH_VLC_PLUGIN=OFF ^
 -DWITH_WINMMDRV=ON ^
 -DWITH_WINMMDRV_PTHREADS=ON ^
 -DWITH_WINMMDRV_MINGWEX=OFF ^
 ..
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build . --config %BUILD_TYPE% -- -j 2
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..


rem ============= Deploy archives =============

cd build-drv-prexp
7z a -t7z -mx9 "libADLMIDI-winmm-driver-prexp.7z" %FILES_LIST%
if %errorlevel% neq 0 exit /b %errorlevel%
appveyor PushArtifact "libADLMIDI-winmm-driver-prexp.7z"
if %errorlevel% neq 0 exit /b %errorlevel%
cd ..

md x86
md x64

cd build-drv-32
7z a -t7z -mx9 "libADLMIDI-winmm-driver-x86.7z" %FILES_LIST%
if %errorlevel% neq 0 exit /b %errorlevel%
appveyor PushArtifact "libADLMIDI-winmm-driver-x86.7z"
if %errorlevel% neq 0 exit /b %errorlevel%
for %%f in (%FILES_LIST%) DO copy %%f ..\x86
cd ..

cd build-drv-64
7z a -t7z -mx9 "libADLMIDI-winmm-driver-x64.7z" %FILES_LIST%
if %errorlevel% neq 0 exit /b %errorlevel%
appveyor PushArtifact "libADLMIDI-winmm-driver-x64.7z"
if %errorlevel% neq 0 exit /b %errorlevel%
for %%f in (%FILES_LIST%) DO copy %%f ..\x64
cd ..

echo @echo off > install-x64.bat
echo cd x86 >> install-x64.bat
echo drvsetup install >> install-x64.bat
echo cd ..\x64 >> install-x64.bat
echo drvsetup install >> install-x64.bat
echo cd .. >> install-x64.bat
echo. >> install-x64.bat

echo @echo off > install-x86.bat
echo cd x86 >> install-x86.bat
echo drvsetup install >> install-x86.bat
echo cd .. >> install-x86.bat
echo. >> install-x86.bat

echo @echo off > uninstall-x64.bat
echo cd x86 >> uninstall-x64.bat
echo drvsetup uninstall >> uninstall-x64.bat
echo cd ..\x64 >> uninstall-x64.bat
echo drvsetup uninstall >> uninstall-x64.bat
echo cd .. >> uninstall-x64.bat
echo. >> install-x64.bat

echo @echo off > uninstall-x86.bat
echo cd x86 >> uninstall-x86.bat
echo drvsetup uninstall >> uninstall-x86.bat
echo cd .. >> uninstall-x86.bat
echo. >> uninstall-x86.bat

7z a -t7z -mx9 "libADLMIDI-winmm-driver-all.7z" *install*.bat x86 x64
if %errorlevel% neq 0 exit /b %errorlevel%
appveyor PushArtifact "libADLMIDI-winmm-driver-all.7z"
if %errorlevel% neq 0 exit /b %errorlevel%

