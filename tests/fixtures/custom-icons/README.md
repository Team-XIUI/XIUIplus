# Raster icon fixtures

These small synthetic images contain opaque red and translucent/opaque blue halves. The GIF's second frame and TIFF's second page are green, so the focused check verifies that static icons use the first frame/page. WebP retains partial alpha.

Regenerate with `python tests/fixtures/custom-icons/generate.py` (Pillow required). These are isolated test inputs, not shipped artwork.
