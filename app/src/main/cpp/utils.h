#pragma once

#include <array>
#include <sys/system_properties.h>

inline auto GetAndroidApiLevel() {
    static auto kApiLevel = []() {
        std::array<char, PROP_VALUE_MAX> prop_value;
        __system_property_get("ro.build.version.sdk", prop_value.data());
        int base = atoi(prop_value.data());
        __system_property_get("ro.build.version.preview_sdk", prop_value.data());
        return base + atoi(prop_value.data());
    }();
    return kApiLevel;
}

bool is_pointer_valid(void *p);
bool is_pointer_valid(void *p, size_t len);
