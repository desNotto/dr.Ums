@echo off
REM Локальная сборка под Windows. Нужны: Visual Studio 2022 (workload "Desktop development with C++"), CMake и Git.
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error
echo.
echo Готово: build\DrUms_artefacts\Release\VST3\dr. Ums.vst3
echo Скопируй папку "dr. Ums.vst3" в C:\Program Files\Common Files\VST3\
goto :eof
:error
echo Сборка не удалась.
exit /b 1
