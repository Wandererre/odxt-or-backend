from fastapi import FastAPI, UploadFile, File, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, FileResponse
from pydantic import BaseModel
import os, csv, re, subprocess, time, io, logging, glob
from typing import List
import PyPDF2

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("inverted_index")

app = FastAPI(title="Inverted Index Builder")

# Directory this script lives in — used as the cwd for the setup/search
# binaries so they reliably find db6k.dat regardless of where uvicorn
# was launched from.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

DATA_DIR        = "data"
os.makedirs(DATA_DIR, exist_ok=True)

WORD_TO_ID_PATH = os.path.join(DATA_DIR, "word_to_id.csv")
ID_TO_WORD_PATH = os.path.join(DATA_DIR, "id_to_word.csv")
DOC_TO_ID_PATH  = os.path.join(DATA_DIR, "doc_to_id.csv")
ID_TO_DOC_PATH  = os.path.join(DATA_DIR, "id_to_doc.csv")
INDEX_PATH      = os.path.join(DATA_DIR, "inverted_index.csv")
DAT_PATH        = "db6k.dat"

# odxt_cli.cpp dispatches setup vs. single-query search by argc (no args ->
# setup, args present -> search over those word_ids), so both point at the
# same compiled binary rather than two separate ones like ntru-oqxt did.
SETUP_BINARY  = "./odxt-cli"
SEARCH_BINARY = "./odxt-cli"

# Keeps the full, untruncated result of the most recent binary invocation so
# it can be inspected via GET /debug/last-run even if you can't easily tail
# the server's terminal/log output.
_last_run: dict = {}


# ── helpers ───────────────────────────────────────────────────────────────────

def load_csv_map(path: str, key_col: str, val_col: str) -> dict:
    result = {}
    if not os.path.exists(path):
        return result
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            result[row[key_col]] = row[val_col]
    return result


def save_two_col_csv(path: str, col1: str, col2: str, data: dict):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([col1, col2])
        for k, v in data.items():
            w.writerow([k, v])


def next_hex_id(existing_ids: set) -> str:
    n = len(existing_ids)
    while True:
        candidate = f"{n:08X}"
        if candidate not in existing_ids:
            return candidate
        n += 1


def tokenize(text: str) -> List[str]:
    return re.findall(r"[a-zA-Z0-9']+", text.lower())


def load_inverted_index() -> dict:
    idx = {}
    if not os.path.exists(INDEX_PATH):
        return idx
    with open(INDEX_PATH, newline="", encoding="utf-8") as f:
        for row in csv.reader(f):
            if not row or row[0] == "word_id":
                continue
            idx[row[0]] = set(row[1:]) if len(row) > 1 else set()
    return idx


def save_inverted_index(idx: dict):
    sorted_items = sorted(idx.items())
    with open(INDEX_PATH, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["word_id", "doc_ids..."])
        for word_id, doc_ids in sorted_items:
            w.writerow([word_id] + sorted(doc_ids))
            
    # Clean write without the trailing comma
    with open(DAT_PATH, "w", encoding="utf-8") as f:
        for word_id, doc_ids in sorted_items:
            f.write(",".join([word_id] + sorted(doc_ids)) + "\n")


def extract_text(filename: str, file_bytes: bytes) -> str:
    """
    Extract plain text from file bytes.
    For real PDFs: PyPDF2 extracts text properly.
    Fallback to raw UTF-8 decode handles plain-text files sent with .pdf
    extension (used by the demo for sample PDFs).
    """
    if filename.lower().endswith(".pdf"):
        try:
            reader = PyPDF2.PdfReader(io.BytesIO(file_bytes))
            text = "".join(page.extract_text() or "" for page in reader.pages)
            if text.strip():
                return text
        except Exception:
            pass
    return file_bytes.decode("utf-8", errors="ignore")


def cleanup_stale_binary_state():
    """
    Unlike the old ntru-oqxt binaries, odxt-cli deliberately persists state
    ACROSS process invocations (update_count.csv, odxt_config.txt, plus
    test_vectors/results scratch dirs) — that's how a separate search
    process, called later, knows about the index a setup process built
    earlier. So on every /upload we wipe exactly those known files to force
    a full rebuild, rather than sweeping unknown glob patterns.

    TODO: if repeated upload-resets ever behave oddly (stale Redis keys from
    a previous demo session bleeding into a new one), add a
    `redis-cli FLUSHALL` here too — odxt_cli.cpp's setup path does not do
    this itself.
    """
    removed = []
    for fname in ["update_count.csv", "odxt_config.txt"]:
        path = os.path.join(BASE_DIR, fname)
        if os.path.exists(path):
            try:
                os.remove(path)
                removed.append(fname)
            except OSError as e:
                log.warning("Could not remove stale artifact %s: %s", path, e)
    for scratch in ["test_vectors/live", "results/live"]:
        for path in glob.glob(os.path.join(BASE_DIR, scratch, "*")):
            try:
                os.remove(path)
                removed.append(os.path.relpath(path, BASE_DIR))
            except OSError as e:
                log.warning("Could not remove stale artifact %s: %s", path, e)
    if removed:
        log.info("Cleaned up stale ODXT state before run: %s", removed)


def run_binary(binary: str, args: List[str], timeout: int = 60) -> dict:
    binary_path = os.path.join(BASE_DIR, binary) if not os.path.isabs(binary) else binary
    if not os.path.isfile(binary_path):
        raise HTTPException(
            status_code=500,
            detail=f"Binary '{binary}' not found in {BASE_DIR}. Place it next to main.py and run: chmod +x {binary}",
        )
    cmd = [binary_path] + args
    log.info("Running: %s (cwd=%s)", " ".join(cmd), BASE_DIR)
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, cwd=BASE_DIR
        )
        stdout = result.stdout.strip()
        stderr = result.stderr.strip()

        # Always log full, untruncated output server-side — this is what
        # "check backend logs" in the frontend banner should actually mean.
        log.info("Exit code: %s", result.returncode)
        if stdout:
            log.info("stdout:\n%s", stdout)
        if stderr:
            log.info("stderr:\n%s", stderr)
        if result.returncode != 0:
            log.error(
                "Binary '%s' exited non-zero (%s). This is a crash/failure inside "
                "the compiled binary itself, not in main.py — the stderr above "
                "(if any) is the only clue we have. Common ODXT-specific causes: "
                "Redis not running, search invoked before any successful setup "
                "(missing odxt_config.txt / update_count.csv), or a bucket_size / "
                "isOptimized mismatch between the setup and search runs.",
                binary, result.returncode,
            )

        combined = stdout
        if stderr:
            combined += "\n--- stderr ---\n" + stderr

        run_info = {
            "command":    " ".join(cmd),
            "exit_code":  result.returncode,
            "output":     combined.strip() or "(binary produced no output)",
            "stdout":     stdout,
            "stderr":     stderr,
        }
        _last_run.update(run_info)
        return run_info
    except FileNotFoundError:
        raise HTTPException(status_code=500, detail=f"Binary not executable: {binary_path}")
    except subprocess.TimeoutExpired:
        log.error("Binary '%s' timed out after %ss", binary, timeout)
        raise HTTPException(status_code=504, detail=f"Binary timed out after {timeout}s")
    except Exception as e:
        log.exception("Unexpected error running binary '%s'", binary)
        raise HTTPException(status_code=500, detail=str(e))


# ── endpoints ─────────────────────────────────────────────────────────────────

@app.post("/upload")
async def upload_files(files: List[UploadFile] = File(...)):
    word_to_id = load_csv_map(WORD_TO_ID_PATH, "word", "id")
    id_to_word = {v: k for k, v in word_to_id.items()}
    doc_to_id  = load_csv_map(DOC_TO_ID_PATH, "doc_name", "id")
    id_to_doc  = {v: k for k, v in doc_to_id.items()}
    inv_index  = load_inverted_index()

    results = []

    for upload in files:
        filename   = upload.filename
        file_bytes = await upload.read()
        content    = extract_text(filename, file_bytes)

        if filename not in doc_to_id:
            doc_id = next_hex_id(set(doc_to_id.values()))
            doc_to_id[filename] = doc_id
            id_to_doc[doc_id]   = filename
        else:
            doc_id = doc_to_id[filename]

        tokens        = tokenize(content)
        unique_tokens = set(tokens)

        for word in unique_tokens:
            if word not in word_to_id:
                wid = next_hex_id(set(word_to_id.values()))
                word_to_id[word] = wid
                id_to_word[wid]  = word
            inv_index.setdefault(word_to_id[word], set()).add(doc_id)

        results.append({
            "filename":     filename,
            "doc_id":       doc_id,
            "token_count":  len(tokens),
            "unique_words": len(unique_tokens),
            "keywords":     sorted(unique_tokens),
            "text":         content,
        })

    save_two_col_csv(WORD_TO_ID_PATH, "word",     "id",       word_to_id)
    save_two_col_csv(ID_TO_WORD_PATH, "id",       "word",     id_to_word)
    save_two_col_csv(DOC_TO_ID_PATH,  "doc_name", "id",       doc_to_id)
    save_two_col_csv(ID_TO_DOC_PATH,  "id",       "doc_name", id_to_doc)
    save_inverted_index(inv_index)

    setup_result = None
    setup_error  = None
    try:
        cleanup_stale_binary_state()
        setup_result = run_binary(SETUP_BINARY, [], timeout=180)
    except HTTPException as e:
        setup_error = e.detail
        log.error("Setup binary raised: %s", e.detail)

    return {
        "status":                 "success",
        "processed":              results,
        "total_words_in_vocab":   len(word_to_id),
        "total_docs_indexed":     len(doc_to_id),
        "inverted_index_entries": len(inv_index),
        "dat_path":               os.path.abspath(DAT_PATH),
        "setup":                  setup_result,
        "setup_error":            setup_error,
    }


@app.get("/debug/last-run")
def debug_last_run():
    """Full, untruncated stdout/stderr/exit code of the most recent odxt-cli
    invocation (setup or search). Use this to see the real crash reason —
    the frontend banner only shows the first 200 chars."""
    if not _last_run:
        return {"message": "No binary has been run yet."}
    return _last_run


@app.get("/stats")
def get_stats():
    word_to_id = load_csv_map(WORD_TO_ID_PATH, "word", "id")
    doc_to_id  = load_csv_map(DOC_TO_ID_PATH, "doc_name", "id")
    inv_index  = load_inverted_index()
    return {
        "total_words":   len(word_to_id),
        "total_docs":    len(doc_to_id),
        "index_entries": len(inv_index),
        "docs":          [{"name": k, "id": v} for k, v in doc_to_id.items()],
        "words_sample":  [{"word": k, "id": v} for k, v in list(word_to_id.items())[:20]],
        "dat_exists":    os.path.isfile(DAT_PATH),
        "dat_path":      os.path.abspath(DAT_PATH),
    }


@app.get("/search")
def search_word(q: str):
    t0 = time.perf_counter()

    word_to_id = load_csv_map(WORD_TO_ID_PATH, "word", "id")
    id_to_doc  = load_csv_map(ID_TO_DOC_PATH,  "id",   "doc_name")
    inv_index  = load_inverted_index()

    word = q.lower().strip()

    if word not in word_to_id:
        ms = round((time.perf_counter() - t0) * 1000, 4)
        return {"found": False, "word": word, "docs": [], "time_taken": ms}

    wid     = word_to_id[word]
    doc_ids = inv_index.get(wid, set())
    docs    = [{"doc_id": did, "doc_name": id_to_doc.get(did, "?")} for did in doc_ids]
    ms      = round((time.perf_counter() - t0) * 1000, 4)

    return {"found": True, "word": word, "word_id": wid, "docs": docs, "time_taken": ms}


class ConjunctiveRequest(BaseModel):
    word_ids: List[str]
    words:    List[str] = []


@app.post("/conjunctive-search")
def conjunctive_search(req: ConjunctiveRequest):
    if not req.word_ids:
        raise HTTPException(status_code=400, detail="No word IDs provided")

    t0     = time.perf_counter()
    result = run_binary(SEARCH_BINARY, req.word_ids, timeout=30)
    ms     = round((time.perf_counter() - t0) * 1000, 4)

    return {
        **result,
        "word_ids":   req.word_ids,
        "words":      req.words,
        "time_taken": ms,
    }


@app.get("/download/{filename}")
def download_file(filename: str):
    allowed = {"word_to_id.csv", "id_to_word.csv", "doc_to_id.csv", "id_to_doc.csv", "inverted_index.csv"}
    if filename not in allowed:
        return JSONResponse(status_code=404, content={"error": "Not found"})
    path = os.path.join(DATA_DIR, filename)
    if not os.path.exists(path):
        return JSONResponse(status_code=404, content={"error": "File not generated yet"})
    return FileResponse(path, filename=filename, media_type="text/csv")


@app.delete("/reset")
def reset():
    for f in ["word_to_id.csv", "id_to_word.csv", "doc_to_id.csv", "id_to_doc.csv", "inverted_index.csv"]:
        p = os.path.join(DATA_DIR, f)
        if os.path.exists(p):
            os.remove(p)
    if os.path.exists(DAT_PATH):
        os.remove(DAT_PATH)
    cleanup_stale_binary_state()
    _last_run.clear()
    return {"status": "reset complete"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8001, reload=True)