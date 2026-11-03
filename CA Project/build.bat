@echo off
echo ========================================================
echo   Building Apex Core Banking Application (C++ and SQLite)
echo ========================================================

REM Terminate any running bank_server instance to avoid file locking on bank_server.exe
taskkill /F /IM bank_server.exe >nul 2>nul

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MinGW GCC / G++ compiler not found in PATH!
    pause
    exit /b 1
)

if not exist sqlite3.o (
    echo [1/4] Compiling SQLite3 engine...
    gcc -O2 -c sqlite3.c -o sqlite3.o
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Failed to compile sqlite3.c
        pause
        exit /b 1
    )
) else (
    echo [1/4] sqlite3.o already up to date.
)

echo [2/4] Compiling C++ Database Engine...
g++ -std=c++11 -O2 -c database.cpp -o database.o
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile database.cpp
    pause
    exit /b 1
)

echo [3/4] Compiling C++ Winsock HTTP Server...
g++ -std=c++11 -O2 -c http_server.cpp -o http_server.o
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile http_server.cpp
    pause
    exit /b 1
)

echo [4/4] Linking bank_server.exe...
g++ -std=c++11 -O2 main.cpp database.o http_server.o sqlite3.o -o bank_server.exe -lws2_32
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Linking failed!
    pause
    exit /b 1
)

echo.
echo ========================================================
echo   [SUCCESS] Build Completed: bank_server.exe
echo   Run 'run.bat' or '.\bank_server.exe' to launch!
echo ========================================================
