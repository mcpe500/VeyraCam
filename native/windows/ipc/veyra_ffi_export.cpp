#include "veyra_ffi_export.h"
#include "../core/service_manager.h"
#include <cstring>
#include <cstdlib>

extern "C" {

VEYRA_API int veyra_core_init(void) {
    bool ok = veyra::ServiceManager::Instance().Initialize();
    return ok ? 0 : -1;
}

VEYRA_API int veyra_core_connect_device(const char* host_ip, uint16_t port,
                                        const char* pin,
                                        void (*status_callback)(const char* status)) {
    if (!host_ip || !pin) return -1;
    bool ok = veyra::ServiceManager::Instance().ConnectDevice(
        host_ip, port, pin,
        status_callback ? [status_callback](const char* s) { status_callback(s); }
                        : veyra::ServiceManager::StatusCallback{});
    return ok ? 0 : -1;
}

VEYRA_API void veyra_core_disconnect_device(void) {
    veyra::ServiceManager::Instance().DisconnectDevice();
}

VEYRA_API void veyra_core_set_zoom(float zoom) {
    veyra::ServiceManager::Instance().SetZoom(zoom);
}

VEYRA_API void veyra_core_set_exposure(int32_t exposure) {
    veyra::ServiceManager::Instance().SetExposure(exposure);
}

VEYRA_API void veyra_core_set_focus(int32_t auto_focus, float distance) {
    veyra::ServiceManager::Instance().SetFocus(auto_focus != 0, distance);
}

VEYRA_API void veyra_core_set_torch(int32_t enabled) {
    veyra::ServiceManager::Instance().SetTorch(enabled != 0);
}

VEYRA_API void veyra_core_switch_camera(int32_t front) {
    veyra::ServiceManager::Instance().SwitchCamera(front != 0);
}

VEYRA_API void veyra_core_request_idr(void) {
    veyra::ServiceManager::Instance().RequestIdr();
}

VEYRA_API uint64_t veyra_core_get_shared_texture_handle(void) {
    return veyra::ServiceManager::Instance().GetSharedTextureHandle();
}

VEYRA_API const char* veyra_core_get_telemetry_json(void) {
    // L-4: bounded static buffer avoids unbounded per-poll heap allocations.
    static thread_local char buffer[4096];
    std::string json = veyra::ServiceManager::Instance().GetTelemetryJson();
    if (json.size() >= sizeof(buffer)) {
        json = json.substr(0, sizeof(buffer) - 1);
    }
    memcpy(buffer, json.c_str(), json.size() + 1);
    return buffer;
}

VEYRA_API void veyra_core_free_string(const char* str) {
    // Telemetry now uses a static buffer; kept for ABI compatibility (no-op).
    (void)str;
}

VEYRA_API void veyra_core_shutdown(void) {
    veyra::ServiceManager::Instance().Shutdown();
}

} // extern "C"
