#include "veyra_ffi_export.h"
#include "../core/service_manager.h"
#include <cstring>
#include <cstdlib>

extern "C" {

VEYRA_API int veyra_core_init(void) {
    bool ok = veyra::ServiceManager::Instance().Initialize();
    return ok ? 0 : -1;
}

VEYRA_API int veyra_core_connect_device(const char* host_ip, uint16_t port) {
    if (!host_ip) return -1;
    bool ok = veyra::ServiceManager::Instance().ConnectDevice(host_ip, port);
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
    std::string json = veyra::ServiceManager::Instance().GetTelemetryJson();
    char* buffer = static_cast<char*>(malloc(json.length() + 1));
    if (buffer) {
        memcpy(buffer, json.c_str(), json.length() + 1);
    }
    return buffer;
}

VEYRA_API void veyra_core_free_string(const char* str) {
    if (str) {
        free(const_cast<char*>(str));
    }
}

VEYRA_API void veyra_core_shutdown(void) {
    veyra::ServiceManager::Instance().Shutdown();
}

} // extern "C"
