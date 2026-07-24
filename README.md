# ODXT OR-Search Backend (Port 8001)

This repo handles the OR (disjunction) searches for the ODXT frontend. It runs its own heavily optimized C++ cryptographic core and listens on port 8001.

**HEADS UP: Dual-Backend Setup**
This is only half of the backend. The React frontend assumes you are running BOTH backends simultaneously:
- Port 8000: The original AND backend 
- Port 8001: This OR backend

If you try to upload a file while the 8000 server is down, the frontend will hang on "uploading..." because it relies on 8000 to extract the initial keywords.

---

## 1. Prerequisites
You need Redis running in the background, plus standard build tools for the C++ code.

sudo apt update && sudo apt install -y redis-server g++ python3-venv
sudo service redis-server start

## 2. Quickstart

Clone this repo and set up your Python environment:

git clone https://github.com/Wandererre/odxt-or-backend.git
cd odxt-or-backend

# Set up and activate virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install Python deps
pip install fastapi uvicorn python-multipart PyPDF2 redis

## 3. Build the C++ Binary
We need to compile the odxt-cli binary before the Python server can do anything. Just run the build script:

./build.sh

(Ignore any compiler warnings about cpu_feature or _GNU_SOURCE — they are harmless).

## 4. Run It
With your .venv activated, start the server:

python3 main.py

It will spin up on http://0.0.0.0:8001. 

To test the full app: Spin up the 8000 backend in another terminal, start your React frontend, and drop a file in.
