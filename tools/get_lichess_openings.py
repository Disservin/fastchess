# Script that gets the latest opening data from lichess-org/chess-openings and updates 'openings_data.hpp' header file

import requests
import zipfile
import io
import os
import csv
import shutil
from pathlib import Path

# === CONFIGURATION ===
REPO = "lichess-org/chess-openings"
WORKFLOW_NAME = "lint.yml"
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")
SCRIPT_DIR = Path(__file__).resolve().parent
SAVE_PATH = SCRIPT_DIR / "tmp"
HEADER_PATH = SCRIPT_DIR.parent / "app" / "src" / "game" / "pgn" / "openings_data.hpp"

# === HEADERS ===
headers = {"Accept": "application/vnd.github+json"}
if GITHUB_TOKEN:
    headers["Authorization"] = f"Bearer {GITHUB_TOKEN}"
else:
    raise Exception(f"No GITHUB_TOKEN found in user's environment.")

# === STEP 1: Get the workflow ID by name ===
workflows_url = f"https://api.github.com/repos/{REPO}/actions/workflows"
resp = requests.get(workflows_url, headers=headers)
resp.raise_for_status()
workflows = resp.json()["workflows"]

workflow = next((w for w in workflows if w["path"].endswith(WORKFLOW_NAME)), None)
if not workflow:
    raise Exception(f"No workflow named '{WORKFLOW_NAME}' found.")

workflow_id = workflow["id"]

# === STEP 2: Get the latest successful run ===
runs_url = f"https://api.github.com/repos/{REPO}/actions/workflows/{workflow_id}/runs"
params = {"status": "success", "per_page": 1}
resp = requests.get(runs_url, headers=headers, params=params)
resp.raise_for_status()
runs = resp.json()["workflow_runs"]

if not runs:
    raise Exception("No successful runs found.")
run_id = runs[0]["id"]
print(f"Latest successful run ID: {run_id}")

# === STEP 3: Get artifacts for that run ===
artifacts_url = f"https://api.github.com/repos/{REPO}/actions/runs/{run_id}/artifacts"
resp = requests.get(artifacts_url, headers=headers)
resp.raise_for_status()
artifacts = resp.json()["artifacts"]

if not artifacts:
    raise Exception("No artifacts found.")

artifact = artifacts[0]
download_url = artifact["archive_download_url"]
artifact_name = artifact["name"]
print(f"Downloading artifact: {artifact_name}")

# === STEP 4: Download and extract ===
resp = requests.get(download_url, headers=headers)
resp.raise_for_status()

os.makedirs(SAVE_PATH, exist_ok=True)
with zipfile.ZipFile(io.BytesIO(resp.content)) as zip_file:
    zip_file.extractall(SAVE_PATH)

print(f"Artifact '{artifact_name}' downloaded and extracted to '{SAVE_PATH}'")

# === STEP 5: Write header file ===
input_file = f"{SAVE_PATH}/all.tsv"
output_file = HEADER_PATH

with open(input_file, encoding="utf-8") as f, open(output_file, "w", encoding="utf-8") as out:
    reader = csv.DictReader(f, delimiter='\t')

    out.write('#pragma once\n')
    out.write('#include <string_view>\n')
    out.write('#include <unordered_map>\n')
    out.write('#include <utility>\n\n')
    out.write('/*\n')
    out.write(' * Data source:\n')
    out.write(' *   https://github.com/lichess-org/chess-openings\n')
    out.write(' *\n')
    out.write(' * License:\n')
    out.write(' *   CC0 1.0 Universal\n')
    out.write(' *   https://creativecommons.org/publicdomain/zero/1.0/\n')
    out.write(' *\n')
    out.write(' * The following data was adapted from the Lichess "chess-openings" project.\n')
    out.write(' * As stated in their repository, the content is released under the CC0 license,\n')
    out.write(' * meaning it is dedicated to the public domain and may be freely used, modified,\n')
    out.write(' * and distributed without restriction.\n')
    out.write(' */\n\n')
    out.write('struct Opening {\n')
    out.write('    std::string_view eco;\n')
    out.write('    std::string_view name;\n')
    out.write('};\n\n')
    out.write('inline const std::unordered_map<std::string_view, Opening> EPD_TO_OPENING = {\n')    

    for row in reader:
        epd = row["epd"].strip().replace('"', r'\"')
        eco = row["eco"].strip().replace('"', r'\"')
        name = row["name"].strip().replace('"', r'\"')
        out.write(f'    {{"{epd}", {{"{eco}", "{name}"}}}},\n')

    out.write('};\n')
    
print(f"{output_file} successfully written.")

if os.path.exists(SAVE_PATH):
    shutil.rmtree(SAVE_PATH)
