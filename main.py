"""SPA-P web app: FastAPI wrapper around the C++ matching program."""
import shutil
import sys
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from parser import parse_data, parse_output

BASE = Path(__file__).parent
SOURCE = BASE / "algo.cpp"
BINARY = BASE / ("algo_bin.exe" if sys.platform.startswith("win") else "algo_bin")
SAMPLE = BASE / "sample_data.txt"
MAX_BYTES = 2_000_000
TIMEOUT_S = 30

app = FastAPI(title="SPA-P Matching")


def ensure_binary() -> Path:
    """Compile algo.cpp the first time (or when it changed)."""
    if not BINARY.exists() or BINARY.stat().st_mtime < SOURCE.stat().st_mtime:
        r = subprocess.run(["g++", "-O2", str(SOURCE), "-o", str(BINARY)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise HTTPException(500, f"Compile failed:\n{r.stderr}")
    return BINARY


def run_matching(text: str) -> dict:
    try:
        info = parse_data(text)
    except Exception as e:
        raise HTTPException(400, f"Could not read data file: {e}")

    binary = ensure_binary()
    with tempfile.TemporaryDirectory() as tmp:
        # the C++ program always reads "data.txt" from its working directory
        (Path(tmp) / "data.txt").write_text(text)
        try:
            r = subprocess.run([str(binary)], cwd=tmp, capture_output=True,
                               text=True, timeout=TIMEOUT_S)
        except subprocess.TimeoutExpired:
            raise HTTPException(504, "Matching took too long.")

    if r.returncode != 0:
        raise HTTPException(400, (r.stderr or r.stdout).strip() or "Program failed.")

    result = parse_output(r.stdout)
    result["info"] = info
    result["raw_output"] = r.stdout
    return result


@app.post("/api/run")
async def run_uploaded(file: UploadFile = File(...)):
    raw = await file.read()
    if len(raw) > MAX_BYTES:
        raise HTTPException(413, "File too large.")
    return run_matching(raw.decode("utf-8", errors="replace"))


@app.post("/api/run-sample")
def run_sample():
    return run_matching(SAMPLE.read_text())


@app.get("/api/sample")
def download_sample():
    return FileResponse(SAMPLE, filename="data.txt")


@app.get("/")
def index():
    return FileResponse(BASE / "static" / "index.html")


app.mount("/static", StaticFiles(directory=BASE / "static"), name="static")
