@echo off

set SHARED_FLAGS=/nologo /Zo /Zi /Oi /GR- /EHa- /WX /W4 /wd4201 /fp:fast /fp:except- -DUNICODE=1 -D_CRT_SECURE_NO_WARNINGS /Iexternal /std:c++17
set MSVC_FLAGS=-DCOMPILER_MSVC=1 
set LLVM_FLAGS=-Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-writable-strings -Wno-missing-field-initializers -Wno-missing-braces -Wno-char-subscripts -Wno-invalid-offsetof -DCOMPILER_LLVM=1 -maes
set SANITIZE_FLAGS=/Od /MT -DTEXTIT_INTERNAL=1 -DTEXTIT_SLOW=1 -fsanitize=address
set DEBUG_FLAGS=/Od /MTd -DTEXTIT_INTERNAL=1 -DTEXTIT_SLOW=1 
set RELEASE_FLAGS=/O2 /MT -DTEXTIT_INTERNAL=0 -DTEXTIT_SLOW=0
set LINKER_FLAGS=/opt:ref /incremental:no
set LINKER_LIBRARIES=user32.lib gdi32.lib dwmapi.lib d3d11.lib d3dcompiler.lib

set FLAGS=%SHARED_FLAGS%
rem echo]
rem if "%1" equ "release" (
rem     echo ------------------------------------------
rem     echo *** BUILDING RELEASE BUILD FROM SOURCE ***
rem     echo ------------------------------------------
rem     set FLAGS=%FLAGS% %RELEASE_FLAGS%
rem ) else if "%1" equ "sanitize" (
rem     echo --------------------------------------------
rem     echo *** BUILDING SANITIZED BUILD FROM SOURCE ***
rem     echo --------------------------------------------
rem     set FLAGS=%FLAGS% %SANITIZE_FLAGS%
rem ) else (
rem     echo ----------------------------------------
rem     echo *** BUILDING DEBUG BUILD FROM SOURCE ***
rem     echo ----------------------------------------
rem     set FLAGS=%FLAGS% %DEBUG_FLAGS%
rem )

if not exist ctm mkdir ctm
misc\ctime.exe -begin ctm\textit.ctm

echo I'm here to be depressed and I'm not out of that at all > textit_lock.temp

if not exist build mkdir build

if "%TEXTIT_USE_LLVM%" equ "1" goto build_llvm

:build_msvc
echo COMPILER: CL
cl code\textit.cpp       -DTEXTIT_BUILD_DLL=1 %FLAGS% %DEBUG_FLAGS% %MSVC_FLAGS% /Fe"textit.dll" /LD /link %LINKER_FLAGS%
cl code\win32_textit.cpp -DTEXTIT_BUILD_DLL=1 %FLAGS% %DEBUG_FLAGS% %MSVC_FLAGS% /Fe"win32_textit_msvc_debug.exe" %LINKER_LIBRARIES% 
echo built win32_textit_msvc_debug.exe
del textit_lock.temp
cl code\win32_textit.cpp code\textit.cpp %FLAGS% %RELEASE_FLAGS% %MSVC_FLAGS% /Fe"win32_textit_msvc_release.exe" /link %LINKER_FLAGS% %LINKER_LIBRARIES%
echo built win32_textit_msvc_release.exe
goto build_finished

:build_llvm
echo COMPILER: CLANG-CL
clang-cl code\textit.cpp -DTEXTIT_BUILD_DLL=1 %FLAGS% %LLVM_FLAGS% /Fe"textit.dll" /LD /link %LINKER_FLAGS%
clang-cl code\win32_textit.cpp %FLAGS% %LLVM_FLAGS% /Fe"win32_textit.exe" %LINKER_LIBRARIES% 
goto build_finished

:build_finished

set LAST_ERROR=%ERRORLEVEL%
del *.lib
del *.obj
del *.ilk
del *.exp
del temp_textit_*.dll

misc\ctime.exe -end ctm\textit.ctm
