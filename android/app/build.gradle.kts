plugins {
    id("com.android.application")
}

android {
    namespace = "com.gpubench.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.gpubench.app"
        minSdk = 21
        targetSdk = 34
        versionCode = 1
        versionName = "0.5.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared"
                )
                cppFlags += listOf("-std=c++11")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
        debug {
            isDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Map project data/ directory into APK assets/
    sourceSets["main"].assets.srcDirs("../../data")

    // Don't compress shader/model/image files — SDL_RWFromFile needs direct access
    androidResources {
        noCompress += listOf(
            "glsl", "vert", "frag", "comp", "tesc", "tese",
            "obj", "mtl", "scene", "png", "ini"
        )
    }
}
