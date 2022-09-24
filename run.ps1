$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Has-Command($name) {
    try {
        Get-Command $name
        return $True
    } catch {
        return $False
    }
}

if (-not ( Has-Command g++ )) {
    choco install -y mingw
}

if (-not (Test-Path "SDL2-devel-2.24.0-mingw")) {
    wget.exe --no-clobber https://github.com/libsdl-org/SDL/releases/download/release-2.24.0/SDL2-devel-2.24.0-mingw.zip
    Expand-Archive SDL2-devel-2.24.0-mingw.zip
}

cp ./SDL2-devel-2.24.0-mingw/SDL2-2.24.0/x86_64-w64-mingw32/bin/SDL2.dll .

# -lmingw32: fix undefined reference to `WinMain'
g++ `
    -std=c++17 `
    -D__WIN32__ `
    "./src/gameboy/emulator/*.cpp" `
    ./src/main.cpp `
    -I./src `
    -I./SDL2-devel-2.24.0-mingw/SDL2-2.24.0/x86_64-w64-mingw32/include `
    -L./SDL2-devel-2.24.0-mingw/SDL2-2.24.0/x86_64-w64-mingw32/lib `
    -static-libstdc++ -static-libgcc `
    -Wall `
    -lmingw32 -lSDL2main -lSDL2 -o main

function something1-download-if-missing($name) {   
    if (-not (Test-Path $name)) {
        something1-file-download.ps1 $name
    }
}

# 32 bit
something1-download-if-missing mspdb140.dll
.\dumpbin.exe /DEPENDENTS /HEADERS .\dumpbin.exe

# 64 bit
something1-download-if-missing libwinpthread-1.dll
.\dumpbin.exe /DEPENDENTS /HEADERS .\SDL2.dll
.\dumpbin.exe /DEPENDENTS /HEADERS .\libwinpthread-1.dll
.\dumpbin.exe /DEPENDENTS /HEADERS .\main.exe

.\main.exe
