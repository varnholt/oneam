"""Select a successful trusted Build run for this exact tagged commit."""
import json
import os
import pathlib
import re
import subprocess
import urllib.request

tag = os.environ['RELEASE_TAG']
if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', tag):
    raise SystemExit('Invalid release tag')
version = re.search(r'\bVERSION\s+([0-9.]+)', pathlib.Path('CMakeLists.txt').read_text())[1]
if tag[1:].split('-')[0] != version:
    raise SystemExit('The release tag must match the CMake project version')
sha = subprocess.check_output(['git', 'rev-parse', f'{tag}^{{commit}}'], text=True).strip()
repository = os.environ['GITHUB_REPOSITORY']
url = f'https://api.github.com/repos/{repository}/actions/workflows/build.yml/runs?head_sha={sha}&status=success&per_page=100'
request = urllib.request.Request(url, headers={
    'Authorization': f'Bearer {os.environ["GH_TOKEN"]}',
    'Accept': 'application/vnd.github+json',
})
with urllib.request.urlopen(request, timeout=30) as response:
    runs = json.load(response)['workflow_runs']
matches = [run for run in runs if run['head_sha'] == sha and run['conclusion'] == 'success'
           and run['event'] in ('push', 'workflow_dispatch')
           and run['head_repository']['full_name'] == repository]
if not matches:
    raise SystemExit('No successful trusted Build run for this exact commit. Wait for CI and retry.')
with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
    output.write(f'run_id={matches[0]["id"]}\n')
print(f'Using verified build {matches[0]["id"]} for {tag} ({sha[:12]})')
