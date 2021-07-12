@echo off

set SHARED_FLAGS=/nologo /Zo /Zi /Oi /GR- /EHa- /WX /W4 /wd4201 /fp:fast /fp:except- -DUNICODE=1 -D_CRT_SECURE_NO_WARNINGS /Iexternal -std:c++17
set MSVC_FLAGS=-DCOMPILER_MSVC=1 
set LLVM_FLAGS=-Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-function -Wno-deprecated-declarations -Wno-writable-strings -Wno-missing-field-initializers -Wno-missing-braces -Wno-char-subscripts -Wno-invalid-offsetof -DCOMPILER_LLVM=1 -maes
set SANITIZE_FLAGS=/Od /MT -DTEXTIT_INTERNAL=1 -DTEXTIT_SLOW=1 -fsanitize=address
set DEBUG_FLAGS=/Od /MTd -DTEXTIT_INTERNAL=1 -DTEXTIT_SLOW=1 
set RELEASE_FLAGS=/O2 /MT
set LINKER_FLAGS=/opt:ref /incremental:no
set LINKER_LIBRARIES=user32.lib gdi32.lib dwmapi.lib

set FLAGS=%SHARED_FLAGS%
echo]
if "%1" equ "release" (
    echo ------------------------------------------
    echo *** BUILDING RELEASE BUILD FROM SOURCE ***
    echo ------------------------------------------
    set FLAGS=%FLAGS% %RELEASE_FLAGS%
) else if "%1" equ "sanitize" (
    echo --------------------------------------------
    echo *** BUILDING SANITIZED BUILD FROM SOURCE ***
    echo --------------------------------------------
    set FLAGS=%FLAGS% %SANITIZE_FLAGS%
) else (
    echo ----------------------------------------
    echo *** BUILDING DEBUG BUILD FROM SOURCE ***
    echo ----------------------------------------
    set FLAGS=%FLAGS% %DEBUG_FLAGS%
)

if not exist ctm mkdir ctm
misc\ctime.exe -begin ctm\textit.ctm

echo I'm here to be depressed and I'm not out of that at all > build\textit_lock.temp

if not exist build mkdir build

if "%TEXTIT_USE_LLVM%" equ "1" goto build_llvm

:build_msvc
pushd build
echo COMPILER: CL
cl ..\code\textit.cpp -DTEXTIT_BUILD_DLL=1 %FLAGS% %MSVC_FLAGS% /Fe"textit.dll" /LD /link %LINKER_FLAGS%
cl ..\code\win32_textit.cpp %FLAGS% %MSVC_FLAGS% /Fe"win32_textit.exe" %LINKER_LIBRARIES% 
popd
goto build_finished

:build_llvm
pushd build
echo COMPILER: CLANG-CL
clang-cl ..\code\textit.cpp -DTEXTIT_BUILD_DLL=1 %FLAGS% %LLVM_FLAGS% /Fe"textit.dll" /LD /link %LINKER_FLAGS%
clang-cl ..\code\win32_textit.cpp %FLAGS% %LLVM_FLAGS% /Fe"win32_textit.exe" %LINKER_LIBRARIES% 
popd
goto build_finished

:build_finished

set LAST_ERROR=%ERRORLEVEL%
del build\textit_lock.temp

misc\ctime.exe -end ctm\textit.ctm
