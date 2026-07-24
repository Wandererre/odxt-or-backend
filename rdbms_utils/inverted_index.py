"""
inverted_index.py

Assigns stable hex IDs to two kinds of entities:
    TCV  – (table_name, column_name, value)   → tcv_id
    TR   – (table_name, row_id)               → tr_id

Builds an inverted index:
    tcv_id  →  {tr_id, tr_id, …}
"""

import csv
import sqlite3
import struct
import sys
from pathlib import Path
from typing import Any

PRIMARY_KEY_COL = "id"
SKIP_TABLES     = {"inverted_index_entries", "inverted_index_meta"}

TCV_TO_ID_FILE = "tcv_to_id.csv"
ID_TO_TCV_FILE = "id_to_tcv.csv"
TR_TO_ID_FILE  = "tr_to_id.csv"
ID_TO_TR_FILE  = "id_to_tr.csv"
INV_CSV_FILE   = "inverted_index.csv"
INV_TXT_FILE   = "inverted_index.txt"
DAT_FILE       = "db6k.dat"

MAGIC = b"DB6K"


def next_hex_id(existing: set) -> str:
    i = 1
    while True:
        cid = f"{i:08x}"
        if cid not in existing:
            return cid
        i += 1


def normalise(value: Any) -> str:
    if value is None:
        return "__null__"
    return str(value).strip().lower()


def _read_csv(path: Path, fieldnames: list) -> list:
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def _write_csv(path: Path, fieldnames: list, rows: list) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(rows)


class TCVMap:
    def __init__(self):
        self._to_id   = {}
        self._from_id = {}

    def load(self, out: Path) -> None:
        for row in _read_csv(out / TCV_TO_ID_FILE, ["table","column","value","id"]):
            key = (row["table"], row["column"], row["value"])
            self._to_id[key]         = row["id"]
            self._from_id[row["id"]] = key

    def save(self, out: Path) -> None:
        fwd = sorted(
            [{"table": t, "column": c, "value": v, "id": i}
             for (t, c, v), i in self._to_id.items()],
            key=lambda r: r["id"]
        )
        _write_csv(out / TCV_TO_ID_FILE, ["table","column","value","id"], fwd)
        rev = [{"id": i, "table": t, "column": c, "value": v}
               for i, (t, c, v) in sorted(self._from_id.items())]
        _write_csv(out / ID_TO_TCV_FILE, ["id","table","column","value"], rev)

    def get_or_create(self, table: str, col: str, val: str) -> str:
        key = (table, col, val)
        if key not in self._to_id:
            new_id = next_hex_id(set(self._to_id.values()))
            self._to_id[key]      = new_id
            self._from_id[new_id] = key
        return self._to_id[key]

    def lookup(self, table: str, col: str, val: str):
        key = (table, col, normalise(val))
        return self._to_id.get(key)

    def label(self, tcv_id: str) -> str:
        if tcv_id not in self._from_id:
            return tcv_id
        t, c, v = self._from_id[tcv_id]
        return f"({t}, {c}, {v!r})"


class TRMap:
    def __init__(self):
        self._to_id   = {}
        self._from_id = {}

    def load(self, out: Path) -> None:
        for row in _read_csv(out / TR_TO_ID_FILE, ["table","row_id","id"]):
            key = (row["table"], int(row["row_id"]))
            self._to_id[key]         = row["id"]
            self._from_id[row["id"]] = key

    def save(self, out: Path) -> None:
        fwd = sorted(
            [{"table": t, "row_id": r, "id": i}
             for (t, r), i in self._to_id.items()],
            key=lambda x: x["id"]
        )
        _write_csv(out / TR_TO_ID_FILE, ["table","row_id","id"], fwd)
        rev = [{"id": i, "table": t, "row_id": r}
               for i, (t, r) in sorted(self._from_id.items())]
        _write_csv(out / ID_TO_TR_FILE, ["id","table","row_id"], rev)

    def get_or_create(self, table: str, row_id: int) -> str:
        key = (table, row_id)
        if key not in self._to_id:
            new_id = next_hex_id(set(self._to_id.values()))
            self._to_id[key]      = new_id
            self._from_id[new_id] = key
        return self._to_id[key]

    def label(self, tr_id: str) -> str:
        if tr_id not in self._from_id:
            return tr_id
        t, r = self._from_id[tr_id]
        return f"({t}, row={r})"

    def resolve(self, tr_id: str):
        return self._from_id.get(tr_id)


def load_inv_csv(path: Path) -> dict:
    index = {}
    if not path.exists():
        return index
    with path.open(encoding="utf-8") as f:
        for line in f:
            parts = [p.strip() for p in line.strip().split(',') if p.strip()]
            if len(parts) >= 2:
                tcv_id, *tr_ids = parts
                index.setdefault(tcv_id, set()).update(tr_ids)
    return index

def save_inv_csv(path: Path, index: dict) -> None:
    with path.open("w", encoding="utf-8") as f:
        for tcv_id, tr_ids in sorted(index.items()):
            f.write(tcv_id + "," + ",".join(sorted(tr_ids)) + ", \n")

def write_inv_txt(path: Path, index: dict, tcv_map: TCVMap, tr_map: TRMap) -> None:
    with path.open("w", encoding="utf-8") as f:
        for tcv_id, tr_ids in sorted(index.items()):
            lhs      = f"{tcv_map.label(tcv_id)}  [id={tcv_id}]"
            postings = ",  ".join(
                f"{tr_map.label(tid)} [id={tid}]" for tid in sorted(tr_ids)
            )
            f.write(f"{lhs}\n    ---> [{postings}]\n\n")

def save_dat(path: Path, index: dict) -> None:
    entries = sorted(index.items())
    with path.open("wb") as f:
        f.write(MAGIC)
        f.write(struct.pack(">I", len(entries)))
        for tcv_id, tr_ids in entries:
            tid_b  = tcv_id.encode()
            post_b = ",".join(sorted(tr_ids)).encode()
            f.write(struct.pack(">H", len(tid_b)));  f.write(tid_b)
            f.write(struct.pack(">H", len(post_b))); f.write(post_b)


def get_user_tables(cur: sqlite3.Cursor) -> list:
    cur.execute("""
        SELECT name FROM sqlite_master
        WHERE type='table' AND name NOT LIKE 'sqlite_%'
        ORDER BY name
    """)
    return [r[0] for r in cur.fetchall() if r[0] not in SKIP_TABLES]

def get_columns(cur: sqlite3.Cursor, table: str) -> list:
    cur.execute(f"PRAGMA table_info('{table}')")
    return [r[1] for r in cur.fetchall() if r[1] != PRIMARY_KEY_COL]


def build_and_persist(db_path: str, out_dir: str) -> dict:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    tcv_map = TCVMap(); tcv_map.load(out)
    tr_map  = TRMap();  tr_map.load(out)
    index   = load_inv_csv(out / INV_CSV_FILE)

    conn   = sqlite3.connect(db_path)
    cur    = conn.cursor()
    tables = get_user_tables(cur)

    if not tables:
        conn.close()
        return {"tables": [], "total_postings": 0}

    total_postings = 0
    table_stats = []

    for table in tables:
        columns = get_columns(cur, table)
        if not columns:
            continue

        col_list = ", ".join([PRIMARY_KEY_COL] + columns)
        cur.execute(f"SELECT {col_list} FROM '{table}'")
        rows = cur.fetchall()

        for row in rows:
            raw_row_id = row[0]
            tr_id = tr_map.get_or_create(table, raw_row_id)
            for col, raw_val in zip(columns, row[1:]):
                val    = normalise(raw_val)
                tcv_id = tcv_map.get_or_create(table, col, val)
                index.setdefault(tcv_id, set()).add(tr_id)
                total_postings += 1

        table_stats.append({"table": table, "rows": len(rows), "cols": len(columns)})

    conn.close()

    tcv_map.save(out)
    tr_map.save(out)
    save_inv_csv(out / INV_CSV_FILE, index)
    save_dat(out / DAT_FILE, index)
    write_inv_txt(out / INV_TXT_FILE, index, tcv_map, tr_map)

    return {
        "tables": table_stats,
        "total_postings": total_postings,
        "unique_tcv": len(tcv_map._to_id),
        "unique_tr": len(tr_map._to_id),
        "index_entries": len(index),
    }


def conjunctive_search_python(tcv_ids: list, out_dir: str) -> dict:
    """Pure-Python conjunctive search over the inverted index."""
    out = Path(out_dir)
    index = load_inv_csv(out / INV_CSV_FILE)
    tr_map = TRMap(); tr_map.load(out)
    tcv_map = TCVMap(); tcv_map.load(out)

    if not tcv_ids:
        return {"hits": [], "count": 0}

    sets = []
    missing = []
    for tcv_id in tcv_ids:
        if tcv_id in index:
            sets.append(index[tcv_id])
        else:
            missing.append(tcv_id)

    if missing:
        return {"hits": [], "count": 0, "missing_ids": missing}

    result_tr_ids = sets[0]
    for s in sets[1:]:
        result_tr_ids = result_tr_ids & s

    hits = []
    for tr_id in sorted(result_tr_ids):
        resolved = tr_map.resolve(tr_id)
        if resolved:
            hits.append({"tr_id": tr_id, "table": resolved[0], "row_id": resolved[1]})

    return {"hits": hits, "count": len(hits)}


def get_db_schema(db_path: str) -> dict:
    conn = sqlite3.connect(db_path)
    cur  = conn.cursor()
    tables = get_user_tables(cur)
    schema = {}
    for table in tables:
        columns = get_columns(cur, table)
        schema[table] = columns
    conn.close()
    return schema


def get_column_values(db_path: str, table: str, column: str) -> list:
    conn = sqlite3.connect(db_path)
    cur  = conn.cursor()
    cur.execute(f"SELECT DISTINCT \"{column}\" FROM \"{table}\" ORDER BY \"{column}\" LIMIT 200")
    vals = [str(r[0]) if r[0] is not None else "__null__" for r in cur.fetchall()]
    conn.close()
    return vals


def tcv_to_id(table: str, column: str, value: str, out_dir: str):
    out = Path(out_dir)
    tcv_map = TCVMap(); tcv_map.load(out)
    return tcv_map.lookup(table, column, value)