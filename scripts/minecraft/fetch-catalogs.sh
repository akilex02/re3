#!/bin/sh
# Unpacks the item catalog MinecraftOSS ships (gzip) beside the downloaded Mojang assets. Run from the game folder.
set -e
SRC="${1:-/home/akilex/Descargas/gtas/2010-rust-rewrite-mashup-main/crates/assets/data/minecraft/item-catalog-26.3.json.gz}"
mkdir -p mcassets
gunzip -c "$SRC" > mcassets/item-catalog-26.3.json
echo "wrote mcassets/item-catalog-26.3.json"
