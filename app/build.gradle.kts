@file:Suppress("UnstableApiUsage")

plugins {
    id("com.android.application")
}

android {
    compileSdk = 33
    defaultConfig {
        applicationId = "org.schabi.stethox"
        minSdk = 15
        targetSdk = 33
        versionCode = 2
        versionName = "1.1"
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
    // implementation "com.facebook.stetho:stetho-js-rhino:1.6.0"
    // implementation "com.facebook.stetho:stetho:1.5.1"
    implementation("io.github.a13e300.stetho:stetho:2.0")
    implementation("io.github.a13e300.stetho:stetho-js-rhino:2.0")
}
