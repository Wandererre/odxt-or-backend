#!/bin/bash
set -e

echo "=========================================="
echo " ODXT OR Backend — Starting up"
echo "=========================================="

# ── Start Redis ───────────────────────────────
echo "[*] Starting Redis server..."
redis-server --daemonize yes --loglevel notice
sleep 1

# ── Wait for Redis to be ready ────────────────
echo "[*] Waiting for Redis to be ready..."
for i in $(seq 1 10); do
    if redis-cli ping | grep -q PONG; then
        echo "[+] Redis is up!"
        break
    fi
    echo "    Attempt $i/10 — Redis not ready yet, waiting..."
    sleep 1
done

# Verify binary exists
if [ ! -f "./odxt-cli" ]; then
    echo "[ERROR] odxt-cli binary not found! Build may have failed."
    exit 1
fi
echo "[+] odxt-cli binary found."

# ── Start FastAPI server ──────────────────────
echo "[*] Starting FastAPI server on port 8001..."
exec python3 main.py
