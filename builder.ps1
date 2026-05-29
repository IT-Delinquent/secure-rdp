$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"
$vcpkgRoot = Join-Path $repoRoot "vcpkg"
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
$toolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$triplet = "x64-windows-static"
$overlayTriplets = Join-Path $repoRoot "cmake\vcpkg-triplets"

if (-not (Test-Path $cmake)) {
    throw "CMake not found at: $cmake"
}

function Ensure-Vcpkg {
    if (-not (Test-Path (Join-Path $vcpkgRoot ".git"))) {
        Write-Host "Cloning vcpkg into $vcpkgRoot ..."
        git clone --depth 1 https://github.com/microsoft/vcpkg $vcpkgRoot
    }
    if (-not (Test-Path $vcpkgExe)) {
        Write-Host "Bootstrapping vcpkg..."
        & (Join-Path $vcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
    }
    if (-not (Test-Path $toolchain)) {
        throw "vcpkg toolchain not found: $toolchain"
    }
}

# If cache points to a different source path (e.g. old repo name), reset build dir.
if (Test-Path $cachePath) {
    $homeLine = Select-String -Path $cachePath -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($homeLine) {
        $cachedSource = $homeLine.Line.Split('=', 2)[1]
        if ($cachedSource -ne $repoRoot) {
            Write-Host "Detected stale CMake cache ($cachedSource). Recreating build directory..."
            Remove-Item -Recurse -Force $buildDir
        }
    }
}

Ensure-Vcpkg

Write-Host "Configuring (vcpkg triplet: $triplet)..."
& $cmake -S $repoRoot -B $buildDir -G "Visual Studio 17 2022" -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_TARGET_TRIPLET=$triplet" `
    "-DVCPKG_OVERLAY_TRIPLETS=$overlayTriplets"

Write-Host "Building Release..."
& $cmake --build $buildDir --config Release --clean-first

$exe = Join-Path $buildDir "Release\TinyRdp.exe"
if (Test-Path $exe) {
    Write-Host "Built: $exe"
} else {
    throw "Build finished but executable not found: $exe"
}
