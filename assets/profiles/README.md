# Portable profile presets

`share-v2.ini` is the fixed dictionary for `XIUI2` share keys, embedded as resource 305. Keep its bytes unchanged: old keys depend on its field order and values. The codec verifies CRC32 `B0E857FE` before using it.

Normal release defaults remain in `config/xiuinative-defaults.ini` and may change independently. A different sharing dictionary needs a new format version and resource while retaining support for XIUI1 and XIUI2.
