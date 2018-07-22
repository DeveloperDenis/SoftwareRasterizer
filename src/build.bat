@echo off

set dll_file_name="SoftwareRasterizer.dll"
set exe_file_name="SoftwareRasterizer.exe"
set pdb_file_name="SoftwareRasterizer"

set hh=%time:~0,2%
if "%time:~0,1%"==" " set hh=0%hh:~1,1%

set dt=%date:~7,2%-%date:~4,2%-%date:~10,4%_%hh%_%time:~3,2%_%time:~6,2%

set denis_library=..\developerdenis\

set flags=/nologo /D_CRT_SECURE_NO_WARNINGS /DDEBUG /DDENIS_WIN32 /Gm- /GR- /EHa- /Zi /FC /W4 /WX /wd4100 /wd4505 /wd4189 /wd4201
set linker_flags=/incremental:no
set includes=/I %denis_library% /I ../src/
set files=..\src\main.cpp

set settings_file="..\src\project.settings"

pushd ..\build\

if exist ..\src\project.settings (
   echo Using user defined project settings

   ..\developerdenis\read_project_settings %settings_file%
   move project_settings.h ..\src\

) else (
   echo Using default project settings...

   REM For simplicity, this should probably be done by calling read_project_settings with no arguments

   echo // THIS IS AN AUTO-GENERATED FILE, MODIFICATIONS IN THIS FILE WILL NOT BE PRESERVED > %settings_file%
   echo. >> %settings_file%
   
   echo #if !defined(PROJECT_SETTINGS_H_^) >> %settings_file%
   echo #define PROJECT_SETTINGS_H_ >> %settings_file%
   echo. >> %settings_file%
   
   echo char* WINDOW_TITLE = "Default Title"; >> %settings_file%
   echo u32 WINDOW_WIDTH = 800; >> %settings_file%
   echo u32 WINDOW_HEIGHT = 600; >> %settings_file%
   echo bool WINDOW_RESIZABLE = true; >> %settings_file%
   echo char* DLL_FILE_NAME = "main.dll"; >> %settings_file%
   echo u32 FPS_TARGET = 60; >> %settings_file%

   echo. >> %settings_file%
   echo #endif >> %settings_file%
)

del *.pdb > NUL 2> NUL

REM build main exe
cl %flags% %includes% %denis_library%\win32_layer.cpp /Fe%exe_file_name% /link %linker_flags% user32.lib Gdi32.lib Shcore.lib

REM build DLL
echo FOO > pdb.lock
cl %flags% %includes% /LD %files% /Fe%dll_file_name% /link /PDB:%pdb_file_name%_%dt%.pdb %linker_flags%
del pdb.lock

popd
