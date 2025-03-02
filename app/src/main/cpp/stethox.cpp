#include <jni.h>

#include "classloader.h"

#include "elf_parser.hpp"
#include "maps_scan.hpp"
#include "logging.h"

#include "art.hpp"
#include "reflection.hpp"

#include <memory>
#include <string>
#include <memory>
#include <array>

static bool initArt(JNIEnv *env) {
    elf_parser::Elf art{};
    uintptr_t art_base;
    std::string art_path;
    maps_scan::MapInfo::ForEach([&](auto &map) -> bool {
        if (map.path.ends_with("/libart.so") && map.offset == 0) {
            art_base = map.start;
            art_path = map.path;
            return false;
        }
        return true;
    });
    if (!art.InitFromFile(art_path, art_base, true)) {
        LOGE("init art");
        return false;
    }
    bool success = true;
    if (!art::Init(env, art)) {
        LOGE("ArtDebugging init failed");
        success = false;
    }
    success &= InitClassLoaders(art);
    return success;
}

extern "C"
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv *env;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4);
    bool success = true;
    success &= Reflection::Init(env);
    success &= initArt(env);
    return success ? JNI_VERSION_1_4 : JNI_ERR;
}
