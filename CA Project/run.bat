@echo off
echo ========================================================
echo   Launching Apex Core Banking Server...
echo ========================================================

if not exist bank_server.exe (
    echo [INFO] bank_server.exe not found. Running build.bat first...
    call build.bat
)

echo Starting HTTP and SQLite Server on port 8080...
start http://localhost:8080/
bank_server.exe 8080
