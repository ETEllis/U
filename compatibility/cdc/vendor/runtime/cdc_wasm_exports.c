#define CDC_NATIVE_NO_MAIN
#include "cdc_native_runtime.c"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define CDC_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define CDC_EXPORT
#endif

CDC_EXPORT int cdc_wasm_replay_json(
    const char *reducer_path,
    const char *surface_path,
    const char *universal_path,
    char *out,
    int out_size) {
    if (out_size <= 0) {
        return -1;
    }
    return cdc_native_replay_json(reducer_path, surface_path, universal_path, out, (size_t)out_size);
}
