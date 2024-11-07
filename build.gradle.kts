plugins {
    alias(libs.plugins.agp.app) apply false
    alias(libs.plugins.agp.lib) apply false
    alias(libs.plugins.kotlin) apply false
}

task("clean", type = Delete::class) {
    delete(rootProject.buildDir)
}
