#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define VEYRA_API __declspec(dllexport)
#else
#define VEYRA_API __attribute__((visibility("default")))
#endif

VEYRA_API int veyra_core_init(void);
VEYRA_API int veyra_core_connect_device(const char* host_ip, uint16_t port);
VEYRA_API void veyra_core_disconnect_device(void);

VEYRA_API void veyra_core_set_zoom(float zoom);
VEYRA_API void veyra_core_set_exposure(int32_t exposure);
VEYRA_API void veyra_core_set_focus(int32_t auto_focus, float distance);
VEYRA_API void veyra_core_set_torch(int32_t enabled);
VEYRA_API void veyra_core_switch_camera(int32_t front);
VEYRA_API void veyra_core_request_idr(void);

VEYRA_API uint64_t veyra_core_get_shared_texture_handle(void);
VEYRA_API const char* veyra_core_get_telemetry_json(void);
VEYRA_API void veyra_core_free_string(const char* str);
VEYRA_API void veyra_core_shutdown(void);

#ifdef __cplusplus
}
#endif
