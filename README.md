# PBL2_JPN

## 1. Cài công cụ và thư viện trong MSYS2 UCRT64

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-pkgconf
pacman -S --needed mingw-w64-ucrt-x86_64-libsodium
pacman -S --needed mingw-w64-ucrt-x86_64-raylib
```

## 2. Build Release trong PowerShell từ thư mục gốc

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
cmake -S . -B build-raylib -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DPBL2_BUILD_RAYLIB=ON -DPBL2_BUILD_EXAMPLE=ON -DBUILD_TESTING=ON
cmake --build build-raylib --parallel 6
```

## 3. Chạy kiểm thử

```powershell
ctest --test-dir build-raylib --output-on-failure
```

## 4. Mở ứng dụng với database đi kèm

```powershell
.\build-raylib\pbl2_app.exe --data-dir ".\database"
```
