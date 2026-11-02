# Vendored dependencies

These sources are compiled into XIUIplus; they are required build inputs, not generated output.

- `minhook/` provides the native hooks used by the host. Its [license](minhook/LICENSE.txt) is embedded in the plugin notices.
- `libwebp/` decodes compressed minimap images. CMake builds the decoder components; its [license](libwebp/COPYING) stays with the source.
- `miniz/` inflates embedded resources through `miniz_tinfl.c`. Its [license](miniz/LICENSE) and pinned source metadata stay with the source.

Build ownership is in the root [CMakeLists.txt](../CMakeLists.txt), [runtime sources](../cmake/runtime-sources.cmake) and [runtime targets](../cmake/runtime-targets.cmake). Keep upstream sources, notices and licenses together when updating a dependency.
