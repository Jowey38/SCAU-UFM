#!/usr/bin/env bash
# Deploys (copies) the SCAU PreProc Workbench plugin into the QGIS 4 default
# profile. Re-run after every plugin edit; QGIS "Plugin Reloader" or a restart
# picks up the new copy. Usage: tools/qgis/deploy_plugin.sh [repo_root]
set -euo pipefail
REPO_ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
SRC="$REPO_ROOT/qgis_plugin/scau_preproc_workbench"
DEST="$APPDATA/QGIS/QGIS4/profiles/default/python/plugins/scau_preproc_workbench"
[ -f "$SRC/metadata.txt" ] || { echo "error: plugin source not found: $SRC" >&2; exit 2; }
mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
cp -r "$SRC" "$DEST"
rm -rf "$DEST/__pycache__"
echo "deployed: $DEST"
