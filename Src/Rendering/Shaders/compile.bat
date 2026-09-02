@echo off
setlocal enabledelayedexpansion

REM Path to glslc
set GLSLC=%~dp0..\..\..\Dependencies\shader_compile\glslc.exe

REM Shader directory (this script's directory)
set SHADER_DIR=%~dp0
cd /d %SHADER_DIR%

REM List of shader file extensions to compile
set EXTENSIONS=vert frag rgen rchit rmiss comp

echo Compiling shaders in %SHADER_DIR%...

for %%E in (%EXTENSIONS%) do (
    for %%F in (*.%%E) do (
        echo Compiling %%F...
        %GLSLC% "%%F" -o "%%F.spv"
        if !errorlevel! neq 0 (
            echo Failed to compile %%F
            exit /b !errorlevel!
        )
    )
)

echo Shader compilation complete.
endlocal