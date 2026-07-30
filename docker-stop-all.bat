@echo off
title Stop All SSE Services (Docker)
echo ===================================================
echo     Stopping Searchable Encryption (SSE) Services
echo ===================================================
echo.

set "ODXT_DIR=%~dp0"

if exist "%ODXT_DIR%docker-compose.yml" (
    echo Stopping ODXT backend...
    docker compose -f "%ODXT_DIR%docker-compose.yml" down
)

if exist "%ODXT_DIR%..\SRIC\docker-compose.yml" (
    echo Stopping SRIC services...
    docker compose -f "%ODXT_DIR%..\SRIC\docker-compose.yml" down
)

docker network rm sse-network >nul 2>&1

echo.
echo All Docker services stopped.
pause
