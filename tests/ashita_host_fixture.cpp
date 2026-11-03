#include <cstdint>

extern "C" auto __stdcall GetInterfaceVersion(void) -> double { return XIUI_TEST_ASHITA_INTERFACE; }
extern "C" auto __stdcall GetAshitaVersion(void) -> uint64_t { return 4ull | (2ull << 16) | (3ull << 48); }
