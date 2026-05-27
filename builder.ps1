cd c:\source\tinyrdp
& "C:\Program Files\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022" -A x64
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --clean-first