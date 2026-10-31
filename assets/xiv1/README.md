# XIV 1.0 theme

`source/` contains small artwork crops from the XIV 1.0 screenshots supplied for this theme. Menu and gauge artwork belongs to Square Enix. These crops are the source inputs; regenerate the atlas with `python scripts/generate-xiv1-theme.py` (Pillow required).

The 256×256 `theme.bgra` atlas is little-endian width/height followed by BGRA pixels. It is embedded as resource 850. `theme.png` is the matching editable preview.

Atlas regions (x, y, width, height):

- Menu corners: (1,1,24,24), (27,1,24,24), (53,1,24,24), (79,1,24,24).
- Top/bottom rails: (1,33,64,8), (1,45,64,8).
- Left/right rails: (69,33,8,64), (81,33,8,64).
- Navy fabric: (160,0,64,64).
- Neutral gauge fill/track: (96,72,64,8), (96,88,64,8).
- Gauge rails: (0,112,64,14); caps: (72,112,9,14), (88,112,9,14).
- Target underline: (0,144,185,16); action medallion: (192,144,59,60).

The refined frame sources come from the complete right-hand menu in the supplied Action Menu screenshot (`codex-clipboard-VG4nXY.png`). All four original 24-pixel corners and complete 8-pixel rails retain their silver outer stroke, dark channel, blue-gray inner stroke and curled details. Rounded silhouette masks remove scene and interior content without color-keying away the inner stroke. Frame patches have one-texel extruded gutters to prevent adjacent artwork bleeding under linear filtering. The default edge setting draws the source at its original size; smaller tooltips scale the complete frame together.

The navy fabric is a clean 32×32 crop at screenshot position (650,105), mirrored into a seamless repeat. The generator supplies neutral gauge fill colors so module resource colors remain tintable. The action medallion reflects its clean rim quadrant to remove the overlapping source cast bar; its center is transparent. Transparent pixels have zero RGB.

The `gauge-*-refined.png` crops come from the later original-game reference showing HP 268 and MP 67 (`codex-clipboard-Hn7C1n.png`, 294×82). Its gauge body is 14 pixels high, with 3-pixel rail bands and 8-pixel fill. The left and right cylindrical caps retain their original 9×14 proportions and separate lighting. The track and rails use row averages to remove photographed scene noise; fill brightness is neutral so the renderer's resource tint stays authoritative.
