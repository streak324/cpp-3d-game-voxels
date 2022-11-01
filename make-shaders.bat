@echo off
for /f "delims=" %%a in ('dir /s /b ".\src\shaders\*"') do (
	echo glslc %%a -o .\spir-v\%%~nxa.spv
	glslc %%a -o .\spir-v\%%~nxa.spv
)

pause