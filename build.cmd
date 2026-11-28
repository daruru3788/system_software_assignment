@echo off
pushd "%~dp0"
if errorlevel 1 exit /b 1
g++ -std=c++17 -O2 -Werror=narrowing -Werror=return-type main.cpp assembler.cpp parser.cpp operand.cpp encoder.cpp value.cpp instruction.cpp layout.cpp output.cpp -o assembler.exe
set "assembler_build_status=%errorlevel%"
if "%assembler_build_status%"=="0" echo Built assembler.exe
popd
exit /b %assembler_build_status%
