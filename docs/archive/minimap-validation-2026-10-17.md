# Minimap validation record — October 17, 2026

> Historical record. Use the [current documentation](../README.md) for implementation guidance.

Historical session notes from before the standalone 1.0.4 updates. Current behavior and remaining acceptance are in [the minimap guide](implementation/28-minimap.md).

Development builds are installed through compatible runtime hot reload. AgentLink confirmed the active development install and a live map/floor with NPC markers in Windurst Waters. The October 17 hover-zoom and independent-text update passed the focused minimap check, including wheel ownership/limits, partial deltas, per-zone persistence, independent text offsets/visibility and profile round trips. AgentLink confirmed runtime **Oct 17 2026 13:58:12** hot-reloaded at **13:58:38** with GPU text and visible HUD. Standalone **1.0.2** includes the WebP/resource-compression changes and passed resource/isolated-startup checks; the later hover-zoom/text controls await the next standalone release. Visual alignment and gameplay acceptance remain for Cody.
