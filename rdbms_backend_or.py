"""
rdbms_backend_or.py
Same as rdbms_backend_and.py (ingestion, schema, filter-resolution logic
all untouched, still from rdbms_utils.inverted_index) except it points at
the odxt-cli binary instead of ntru-oqxt, on its own port (8003) so it can
run alongside the doc-based OR backend (8001) and RDBMS-AND (8002).

odxt-cli dispatches setup vs. search by argc (same convention as the
doc-based OR backend) - no args -> setup, args -> search - so both
SETUP_BINARY and SEARCH_BINARY point at the same executable here too.

NOTE: odxt-cli's stdout is plain text (Searching for / N IDs TSet / Nmatch /
Search time), not JSON, so run_binary()'s json.loads() will fail and fall
through to its {stdout, stderr, returncode} fallback branch - expected,
already handled.
"""

import csv
import re
import shutil
import sqlite3
import subprocess
import traceback
import json
from pathlib import Path
from typing import List, Optional

from fastapi import FastAPI, File, UploadFile, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, HTMLResponse
# from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from rdbms_utils.inverted_index import build_and_persist, normalise, TCVMap

app = FastAPI(title="Inverted Index Explorer")

app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

UPLOAD_DIR    = Path("./uploads")
INDEX_DIR     = Path("./rdbms_test")

SETUP_BINARY    = "./odxt-cli"
SEARCH_BINARY   = "./odxt-cli"

# Tracks the most recently uploaded .db so /rows can read straight from it.
_last_db_path: Optional[Path] = None

UPLOAD_DIR.mkdir(exist_ok=True)
INDEX_DIR.mkdir(exist_ok=True)


# ── Binary runner ──────────────────────────────────────────────────────────────

def run_binary(binary: str, args: list, timeout: int = 30) -> dict:
    if not Path(binary).is_file():
        return {"error": f"Binary '{binary}' not found in {Path.cwd()}. "
                          f"Place it next to this script and: chmod +x {binary}"}
    try:
        result = subprocess.run(
            [binary] + args,
            capture_output=True, text=True, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        return {"error": f"'{binary}' timed out after {timeout}s"}
    except OSError as e:
        return {"error": f"Could not run '{binary}': {e}"}
    try:
        return json.loads(result.stdout)
    except Exception:
        return {"stdout": result.stdout, "stderr": result.stderr, "returncode": result.returncode}


# ── TCV lookup from CSV only ───────────────────────────────────────────────────

def resolve_filters_to_ids(filters: list) -> tuple:
    """Convert [{table, column, value}] → (tcv_ids, missing) using CSV only."""
    tcv_map = TCVMap()
    tcv_map.load(INDEX_DIR)

    tcv_ids = []
    missing = []
    for f in filters:
        norm = normalise(f.get("value", ""))
        tid  = tcv_map.lookup(f["table"], f["column"], norm)
        if tid:
            tcv_ids.append(tid)
        else:
            missing.append(f)
    return tcv_ids, missing


def schema_from_csv() -> dict:
    """Tables + columns from tcv_to_id.csv — no SQLite."""
    path = INDEX_DIR / "tcv_to_id.csv"
    if not path.exists():
        return {}
    schema: dict = {}
    with path.open(encoding="utf-8") as f:
        for row in csv.DictReader(f):
            schema.setdefault(row["table"], set()).add(row["column"])
    return {t: sorted(cols) for t, cols in schema.items()}


def values_from_csv(table: str, column: str) -> list:
    """Distinct values for (table, column) from tcv_to_id.csv — no SQLite."""
    path = INDEX_DIR / "tcv_to_id.csv"
    if not path.exists():
        return []
    vals = []
    with path.open(encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["table"] == table and row["column"] == column:
                vals.append(row["value"])
    return sorted(vals)


# ── CSV → SQLite ────────────────────────────────────────────────────────────
# The frontend uploads plain CSVs (one per table) instead of a .db file.
# build_and_persist() needs real SQLite though, so we build one here first -
# every CSV becomes one table, header row -> column names, every column
# stored as TEXT (avoids type-inference mismatches). This is the ONLY sqlite
# query this file runs directly: everything past this point is still
# build_and_persist()'s own SQL against the resulting db, unchanged.

def _sanitize_ident(name: str) -> str:
    # table/column names come from user-controlled filenames/headers.
    # build_and_persist() builds its own SELECT column list WITHOUT quoting
    # identifiers, so anything with a space (or other SQL-special char)
    # breaks it (e.g. "employee id" unquoted parses as "SELECT employee AS
    # id", not one column - "no such column: employee"). So this has to be
    # stricter than just "valid inside double quotes": collapse whitespace
    # and anything non-alphanumeric to underscores so the identifier is
    # safe unquoted too.
    name = re.sub(r'"', '', name).strip()
    name = re.sub(r'\s+', '_', name)
    name = re.sub(r'[^A-Za-z0-9_]', '_', name)
    return name or "col"

def build_sqlite_from_csvs(files_content: dict, db_path: Path) -> list:
    """files_content: {table_name: csv_text}. Returns the list of executed
    CREATE TABLE statements, for transparency/debugging."""
    if db_path.exists():
        db_path.unlink()
    con = sqlite3.connect(str(db_path))
    cur = con.cursor()
    ddl_log = []
    for table_name, csv_text in files_content.items():
        reader = csv.reader(csv_text.splitlines())
        rows = list(reader)
        if not rows:
            continue
        headers = [_sanitize_ident(h) for h in rows[0]]
        data_rows = rows[1:]

        # build_and_persist() requires an "id" column on every table - add
        # one (sequential, 0-based) if the CSV doesn't already have it.
        has_id = any(h.lower() == "id" for h in headers)
        if not has_id:
            headers = ["id"] + headers
            data_rows = [[str(i)] + list(r) for i, r in enumerate(data_rows)]

        tname = _sanitize_ident(table_name)
        col_defs = ", ".join(f'"{h}" TEXT' for h in headers)
        ddl = f'CREATE TABLE "{tname}" ({col_defs})'
        cur.execute(ddl)
        ddl_log.append(ddl)

        placeholders = ", ".join(["?"] * len(headers))
        # pad/truncate ragged rows so executemany doesn't choke on a
        # malformed CSV row instead of just ignoring the extra/missing cells
        clean_rows = [
            (r + [""] * len(headers))[:len(headers)] for r in data_rows
        ]
        if clean_rows:
            cur.executemany(f'INSERT INTO "{tname}" VALUES ({placeholders})', clean_rows)

    con.commit()
    con.close()
    return ddl_log


# ── Upload & Index ─────────────────────────────────────────────────────────────

@app.post("/upload")
async def upload_database(files: List[UploadFile] = File(...)):
    global _last_db_path
    try:
        files_content = {}
        for f in files:
            if not f.filename.lower().endswith(".csv"):
                raise HTTPException(status_code=400, detail=f"'{f.filename}': only .csv files accepted.")
            raw = await f.read()
            table_name = Path(f.filename).stem
            files_content[table_name] = raw.decode("utf-8", errors="ignore")

        dest = UPLOAD_DIR / "uploaded.db"
        try:
            ddl_log = build_sqlite_from_csvs(files_content, dest)
        except Exception as e:
            raise HTTPException(
                status_code=500,
                detail=f"Could not build SQLite db from uploaded CSVs: {e}\n{traceback.format_exc()}",
            )
        _last_db_path = dest

        shutil.rmtree(INDEX_DIR, ignore_errors=True)
        INDEX_DIR.mkdir()

        try:
            stats = build_and_persist(str(dest), str(INDEX_DIR))
        except Exception as e:
            raise HTTPException(
                status_code=500,
                detail=f"build_and_persist() failed: {e}\n{traceback.format_exc()}",
            )

        inv_path = INDEX_DIR / "inverted_index.csv"
        if not inv_path.exists():
            raise HTTPException(
                status_code=500,
                detail=f"build_and_persist() ran without error but did not produce {inv_path}. "
                       f"Files actually in {INDEX_DIR}: {[p.name for p in INDEX_DIR.glob('*')]}",
            )
        shutil.copy(inv_path, "db6k.dat")

        setup_result = run_binary(SETUP_BINARY, [], timeout=120)

        return {
            "filename": ", ".join(files_content.keys()),
            "tables_created": ddl_log,
            "stats": stats,
            "setup": setup_result,
        }
    except HTTPException:
        raise
    except Exception as e:
        # last-resort catch so nothing ever comes back as a bare 500 with no body
        raise HTTPException(status_code=500, detail=f"Unexpected error: {e}\n{traceback.format_exc()}")


@app.get("/status")
def status():
    return {"index_ready": (INDEX_DIR / "tcv_to_id.csv").exists()}


@app.get("/rows")
def rows(table: str):
    """
    Real row content for one table, read directly from the uploaded .db via
    stdlib sqlite3 - independent of rdbms_utils.inverted_index. Used by the
    frontend to build its local "expected result" index (same pattern as the
    doc-based tabs: real crypto binary for timing, local plaintext knowledge
    for what the client already legitimately knows).
    """
    if _last_db_path is None or not _last_db_path.exists():
        raise HTTPException(status_code=400, detail="No database uploaded yet.")
    schema = schema_from_csv()
    if table not in schema:
        raise HTTPException(status_code=404, detail=f"Unknown table '{table}'")
    try:
        con = sqlite3.connect(str(_last_db_path))
        cur = con.execute(f'SELECT * FROM "{table}"')
        headers = [d[0] for d in cur.description]
        data = [["" if v is None else str(v) for v in r] for r in cur.fetchall()]
        con.close()
        return {"table": table, "headers": headers, "rows": data}
    except sqlite3.Error as e:
        raise HTTPException(status_code=400, detail=f"Could not read table '{table}': {e}")


# ── Schema & Values (CSV only, no SQLite) ─────────────────────────────────────

@app.get("/schema")
def schema():
    s = schema_from_csv()
    if not s:
        raise HTTPException(status_code=400, detail="No index found.")
    return s


@app.get("/values")
def values(table: str, column: str):
    return {"values": values_from_csv(table, column)}


# ── Conjunctive Search ─────────────────────────────────────────────────────────

class ConjunctiveRequest(BaseModel):
    word_ids: List[str]
    words:    List[str] = []


@app.post("/conjunctive-search")
def conjunctive_search(req: ConjunctiveRequest):
    if not req.word_ids:
        raise HTTPException(status_code=400, detail="No word IDs provided")

    result = run_binary(SEARCH_BINARY, req.word_ids, timeout=30)
    return {
        **result,
        "word_ids": req.word_ids,
        "words":    req.words,
    }


# ── /search: resolve (table,col,val) filters → ids → binary ───────────────────

class FilterRequest(BaseModel):
    filters: List[dict]   # [{table, column, value}, ...]


@app.post("/search")
def search(req: FilterRequest):
    if not req.filters:
        raise HTTPException(status_code=400, detail="No filters provided.")

    tcv_ids, missing = resolve_filters_to_ids(req.filters)

    if missing:
        raise HTTPException(status_code=400, detail=f"Could not resolve: {missing}")

    result = run_binary(SEARCH_BINARY, tcv_ids, timeout=30)
    return {
        **result,
        "word_ids": tcv_ids,
        "words":    [f"({f['table']},{f['column']},{f['value']})" for f in req.filters],
    }


# ── Downloads ──────────────────────────────────────────────────────────────────

@app.get("/download/{filename}")
def download_file(filename: str):
    allowed = {"tcv_to_id.csv","id_to_tcv.csv","tr_to_id.csv","id_to_tr.csv",
               "inverted_index.csv","inverted_index.txt","db6k.dat"}
    if filename not in allowed:
        raise HTTPException(status_code=404)
    path = INDEX_DIR / filename
    if not path.exists():
        raise HTTPException(status_code=404, detail="File not found.")
    return FileResponse(str(path), filename=filename)


# ── Frontend ───────────────────────────────────────────────────────────────────

# @app.get("/", response_class=HTMLResponse)
# def serve_frontend():
#     return HTMLResponse(Path("./static/index.html").read_text())

# app.mount("/static", StaticFiles(directory="./static"), name="static")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("rdbms_backend_or:app", host="0.0.0.0", port=8003, reload=True)