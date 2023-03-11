buildscript {
    
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath("com.android.tools.build:gradle:7.4.2")
    }
}

allprojects {
    repositories {
        mavenLocal()
        google()
        mavenCentral()
        maven("https://api.xposed.info/")
    }
}

task("clean", type = Delete::class) {
    delete(rootProject.buildDir)
}
