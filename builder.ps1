$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"

# If cache points to a different source path (e.g. old repo name), reset build dir.
if (Test-Path $cachePath) {
    $homeLine = Select-String -Path $cachePath -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($homeLine) {
        $cachedSource = $homeLine.Line.Split('=', 2)[1]
        if ($cachedSource -ne $repoRoot) {
            Write-Host "Detected stale CMake cache ($cachedSource). Recreating build directory..."
            Remove-Item -Recurse -Force $buildDir
        }
    }
}

& $cmake -S $repoRoot -B $buildDir -G "Visual Studio 17 2022" -A x64
& $cmake --build $buildDir --config Release --clean-first