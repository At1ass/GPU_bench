#!/bin/sh
# Download Stanford Bunny OBJ model

DEST="data/models/bunny.obj"
mkdir -p data/models

# Stanford mdfisher: ~2.5K verts, clean OBJ
URL="https://graphics.stanford.edu/~mdfisher/Data/Meshes/bunny.obj"
# Fallback: full-resolution from GitHub (~35K verts)
URL2="https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/stanford-bunny.obj"

echo "Downloading Stanford Bunny..."

download() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$DEST" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$DEST" "$1"
    else
        echo "Error: curl or wget required"
        return 1
    fi
}

download "$URL" || download "$URL2"

if [ -s "$DEST" ]; then
    VERTS=$(grep -c "^v " "$DEST" || true)
    FACES=$(grep -c "^f " "$DEST" || true)
    echo "OK: $DEST ($VERTS vertices, $FACES faces)"
else
    rm -f "$DEST"
    echo "Failed to download bunny.obj"
    echo "You can manually place any .obj file at $DEST"
    exit 1
fi
