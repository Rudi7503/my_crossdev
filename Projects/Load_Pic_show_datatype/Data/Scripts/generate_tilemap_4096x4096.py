#!/usr/bin/env python3
# generate_tilemap_4096x4096.py
#
# Erzeugt eine Tilemap-Datei fuer einen 4096x4096 Pixel Screen
# mit 32x32 Pixel Tiles (16-bit Farbe).
#
# Screen:      4096 x 4096 Pixel
# Tile-Groesse: 32 x 32 Pixel
# Tiles pro Zeile:   4096 / 32 = 128
# Tiles pro Spalte:  4096 / 32 = 128
# Tiles gesamt:      128 x 128 = 16384
#
# Ausgabe: tilemap_4096x4096.txt
#   - Eine Zeile pro Tile-Reihe (128 Reihen)
#   - 128 Tile-Nummern (0-9) pro Zeile, Komma-getrennt
#   - Tile-Nummer zyklisch: (row * cols + col) % 10

SCREEN_W  = 4096
SCREEN_H  = 4096
TILE_W    = 32
TILE_H    = 32
NUM_TILES = 10  # Tile-Nummern 0-9

cols = SCREEN_W // TILE_W   # 128
rows = SCREEN_H // TILE_H   # 128
total = cols * rows          # 16384

output_file = "tilemap_4096x4096.txt"

with open(output_file, "w") as f:
    for row in range(rows):
        line_nums = []
        for col in range(cols):
            tile_index = (row * cols + col) % NUM_TILES
            line_nums.append(str(tile_index))
        f.write(",".join(line_nums) + "\n")

print(f"Tilemap erzeugt: {output_file}")
print(f"  Screen:      {SCREEN_W} x {SCREEN_H} Pixel")
print(f"  Tile-Groesse: {TILE_W} x {TILE_H} Pixel")
print(f"  Tiles:       {cols} x {rows} = {total} gesamt")
print(f"  Tile-Nummern: 0-{NUM_TILES - 1} (zyklisch)")
