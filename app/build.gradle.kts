@file:Suppress("UnstableApiUsage")

import java.io.BufferedReader
import java.io.FileInputStream
import java.io.FileReader
import java.io.FileWriter
import java.util.Properties

plugins {
    alias(libs.plugins.agp.app)
    alias(libs.plugins.kotlin)
}

val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = if (keystorePropertiesFile.exists() && keystorePropertiesFile.isFile) {
    Properties().apply {
        load(FileInputStream(keystorePropertiesFile))
    }
} else null

android {
    namespace = "io.github.a13e300.tools"
    compileSdk = 35
    signingConfigs {
        if (keystoreProperties != null) {
            create("release") {
                keyAlias = keystoreProperties["keyAlias"] as String
                keyPassword = keystoreProperties["keyPassword"] as String
                storeFile = file(keystoreProperties["storeFile"] as String)
                storePassword = keystoreProperties["storePassword"] as String
            }
        }
    }
    buildFeatures {
        buildConfig = true
        prefab = true
    }
    defaultConfig {
        applicationId = "io.github.a13e300.tools.stethox"
        minSdk = 24
        targetSdk = 35
        versionCode = 2
        versionName = "1.0.2"
        externalNativeBuild {
            cmake {
                cppFlags("-std=c++20", "-fno-rtti", "-fno-exceptions")
                arguments += "-DANDROID_STL=none"
            }
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs["debug"]
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    kotlinOptions {
        jvmTarget = "11"
    }
}

task("generateDefaultOkHttp3Helper") {
    group = "StethoX"
    doFirst {
        runCatching {
            val okioTypes = listOf("BufferedSink", "Sink", "BufferedSource", "Okio", "Source", "BufferedSource")
            fun String.toOkhttpClass() =
                if (this.contains("_")) this.replace("_", ".")
                else if (this in okioTypes) "okio.$this"
                else "okhttp3.$this"
            val f =
                project.file("src/main/java/io/github/a13e300/tools/network/okhttp3/OkHttp3Helper.java")
            FileWriter(project.file("src/main/java/io/github/a13e300/tools/network/okhttp3/DefaultOkHttp3Helper.java")).use { writer ->
                BufferedReader(FileReader(f)).use { reader ->
                    val cstrContent = StringBuilder()
                    for (l in reader.lines()) {
                        if (l.startsWith("package") || l.startsWith("import")) {
                            writer.write(l)
                        } else if (l.startsWith("public interface")) {
                            writer.write("import java.lang.reflect.*;\n")
                            writer.write("import io.github.a13e300.tools.deobfuscation.NameMap;\n")
                            writer.write("public class DefaultOkHttp3Helper implements OkHttp3Helper {")
                        } else {
                            val trimmed = l.trim()
                            if (trimmed.startsWith("}") || trimmed.startsWith("//")) continue
                            val r = Regex("\\s*(.*)\\((.*)\\)(.*);").matchEntire(l) ?: continue
                            val typeAndName = r.groupValues[1].split(" ")
                            val arguments = r.groupValues[2]
                            val exceptions = r.groupValues[3].trim().let {
                                if (!it.startsWith("throws")) emptyList()
                                else it.removePrefix("throws").trim().split(Regex("\\s+,\\s+"))
                            }
                            var isStatic: Boolean = false
                            var returnType: String? = null
                            var className: String? = null
                            var methodName: String? = null
                            var functionName: String? = null
                            var isMethodGetter = false
                            var isClassGetter = false
                            var isFieldGetter = false
                            for (token in typeAndName) {
                                if (token == "/*static*/") {
                                    isStatic = true
                                    continue
                                }
                                if (returnType == null) {
                                    returnType = token
                                    if (token == "Method") isMethodGetter = true
                                    if (token == "Class") isClassGetter = true
                                    if (token == "Field") isFieldGetter = true
                                } else {
                                    if (isClassGetter) {
                                        className = token.toOkhttpClass()
                                        functionName = token
                                    } else Regex("(.*)_\\d*(.*?)").matchEntire(token)?.let {
                                        className = it.groupValues[1].toOkhttpClass()
                                        methodName = it.groupValues[2]
                                        functionName = token
                                    }
                                }
                            }
                            val argTypesAndNames = arguments.let {
                                if (isMethodGetter) it.removePrefix("/*").removeSuffix("*/")
                                else it
                            }.split(Regex(",\\s*")).mapNotNull { token ->
                                if (token.isEmpty()) return@mapNotNull null
                                token.split(" ").let {
                                    it[it.lastIndex - 1].let { t ->
                                        Regex("/\\*(.*)\\*/").matchEntire(t)?.groupValues?.get(1)
                                            ?.let { t_ ->
                                                t_.toOkhttpClass() to false
                                            } ?: (t to true)
                                    } to it.last()
                                }
                            }
                            if (isClassGetter) {
                                writer.write("private Class class_$functionName;\n")
                                writer.write("@Override public")
                                writer.write(l.removeSuffix(";") + " {\nreturn class_$functionName;\n}\n")
                                cstrContent.append("class_$functionName = classLoader.loadClass(nameMap.getClass(\"$className\"));\n")
                                continue
                            } else if (isFieldGetter) {
                                writer.write("private Field field_$functionName;\n")
                                writer.write("@Override public")
                                writer.write(l.removeSuffix(";") + " {\nreturn field_$functionName;\n}\n")
                                cstrContent.append("field_$functionName = classLoader.loadClass(nameMap.getClass(\"$className\")).getDeclaredField(nameMap.getField(\"$className\", \"$methodName\"));\n")
                                cstrContent.append("field_$functionName.setAccessible(true);\n")
                                continue
                            }
                            writer.write("private Method method_$functionName;\n")
                            writer.write("@Override public ")
                            writer.write(l.removeSuffix(";") + " {\n")
                            if (isMethodGetter) {
                                writer.write("return method_$functionName;\n}")
                            } else {
                                writer.write("try {\n${if (returnType == "void") "" else "return ($returnType)"}method_$functionName.invoke(")
                                val argList = mutableListOf<String>()
                                if (isStatic) argList.add("null")
                                for ((_, name) in argTypesAndNames) {
                                    argList.add(name)
                                }
                                writer.write(argList.joinToString(", "))
                                writer.write(");\n}")
                                if (exceptions.isNotEmpty()) {
                                    writer.write("catch (InvocationTargetException ex) {\n")
                                    writer.write(exceptions.map { ex ->
                                        "if (ex.getCause() instanceof $ex) { throw ($ex) ex.getCause(); }\n"
                                    }.joinToString(" else "))
                                    writer.write(" else throw new RuntimeException(ex);\n}")
                                }
                                writer.write("catch (Throwable t) { throw new RuntimeException(t); }\n}")
                            }
                            cstrContent.append("method_$functionName = classLoader.loadClass(nameMap.getClass(\"$className\"))")
                                .append(".getDeclaredMethod(nameMap.getMethod(\"$className\", \"$methodName\"")
                                .apply {
                                    var first = true
                                    for ((t, _) in argTypesAndNames) {
                                        if (!isStatic && first) {
                                            first = false
                                            continue
                                        }
                                        val (type, _) = t
                                        append(", \"")
                                        if (type == "String") append("java.lang.String")
                                        else append(type)
                                        append("\"")
                                    }
                                    append(")")
                                }
                            var first = true
                            for ((t, _) in argTypesAndNames) {
                                val (type, known) = t
                                if (!isStatic && first) {
                                    first = false
                                    continue
                                }
                                cstrContent.append(",")
                                if (known)
                                    cstrContent.append("$type.class")
                                else
                                    cstrContent.append("classLoader.loadClass(\"$type\")")
                            }
                            cstrContent.append(");\nmethod_$functionName.setAccessible(true);\n")
                        }
                        writer.write("\n")
                    }
                    writer.write("")
                    writer.write("public DefaultOkHttp3Helper(ClassLoader classLoader, NameMap nameMap) {\n")
                    writer.write("try {\n")
                    writer.write(cstrContent.toString())
                    writer.write("} catch (Throwable t) {\nthrow new RuntimeException(t);\n}\n}\n}")
                }
            }
        }.onFailure { it.printStackTrace() }
    }
}

dependencies {
    implementation(libs.annotation)
    compileOnly(libs.api)
    compileOnly(project(":hidden-api"))
    implementation(libs.stetho)
    implementation(libs.stetho.js.rhino)
    implementation(libs.stetho.urlconnection)
    implementation(libs.rhino)
    implementation(libs.dexmaker)
    implementation(libs.dexkit)
    implementation(libs.cxx)
}
