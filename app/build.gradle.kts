@file:Suppress("UnstableApiUsage")

plugins {
    id("com.android.application")
}

android {
    namespace = "io.github.a13e300.tools"
    compileSdk = 33
    defaultConfig {
        applicationId = "io.github.a13e300.tools.stethox"
        minSdk = 15
        targetSdk = 33
        versionCode = 1
        versionName = "1.0"
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
        }
    }
}

dependencies {
    compileOnly("de.robv.android.xposed:api:82")
    implementation("io.github.a13e300.stetho:stetho:2.0")
    implementation("io.github.a13e300.stetho:stetho-js-rhino:2.0")
}
