$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    g++ -std=c++17 -O2 -Werror=narrowing -Werror=return-type main.cpp assembler.cpp parser.cpp operand.cpp encoder.cpp value.cpp instruction.cpp layout.cpp output.cpp -o assembler.exe
    if ($LASTEXITCODE -ne 0) {
        throw "C++ build failed"
    }
    Write-Output "Built assembler.exe"
} finally {
    Pop-Location
}
