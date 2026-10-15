// ================================================================
// 基于 processing-library-template 改造：Java 侧发布构建脚本。
// 该脚本负责把 Java 代码打成 cpplib.jar，并把
//   - library/native/**（由 CMake 构建好的 dll，见 tools/build.ps1）
//   - examples/**、src/** 等
// 组装成可安装到 Processing sketchbook 的标准类库目录。
//
// “USER BUILD CONFIGURATIONS”区域可按需修改，其余保持默认即可。
// ================================================================

import java.util.Properties
import org.gradle.internal.os.OperatingSystem

plugins {
    id("java")
}

java {
    toolchain {
        // Processing 4 用 Java 17 编译，这里保持一致
        languageVersion = JavaLanguageVersion.of(17)
    }
}

// 读取 release.properties（会写入最终 library.properties）
val libraryProperties = Properties().apply {
    load(rootProject.file("release.properties").inputStream())
}

version = if (project.hasProperty("githubReleaseTag")) {
    project.property("githubReleaseTag").toString().drop(1)
} else {
    libraryProperties.getProperty("prettyVersion")
}

// ==========================
// USER BUILD CONFIGURATIONS
// ==========================

// 库短名：影响 jar/zip/安装目录命名（只允许小写字母数字和 '-'）
val libName = "cpplib"

// groupId（你的组织域名反写），同时对应 Java 包名（当前是 org.example.cpplib）
group = "org.example"

// 你的 Processing sketchbook 目录（安装/调试库用）。如果你改了 sketchbook 路径，
// 在 Processing 里 Preferences 查看并修改下面这行。
var sketchbookLocation = ""
val userHome = System.getProperty("user.home")
val currentOS = OperatingSystem.current()
if (currentOS.isMacOsX) {
    sketchbookLocation = if (File("$userHome/Documents/Processing/sketchbook").isDirectory) {
        "$userHome/Documents/Processing/sketchbook"
    } else {
        "$userHome/Documents/Processing"
    }
} else if (currentOS.isWindows) {
    val docsFolder = if (File("$userHome/My Documents").isDirectory) {
        "$userHome/My Documents"
    } else {
        "$userHome/Documents"
    }
    sketchbookLocation = if (File(docsFolder, "Processing/sketchbook").isDirectory) {
        "$docsFolder/Processing/sketchbook"
    } else {
        "$docsFolder/Processing"
    }
} else {
    sketchbookLocation = "$userHome/sketchbook"
}

repositories {
    mavenCentral()
    maven { url = uri("https://jogamp.org/deployment/maven/") }
}

// ---- 依赖 -------------------------------------------------------
// Processing core 优先使用本机安装（Processing 自带的 JDK 与 core 版本最匹配）；
// 找不到时才退回到 Maven Central 的 org.processing:core。
val processingHome = System.getenv("PROCESSING_HOME") ?: "C:/Program Files/Processing"
val processingCoreDir = file("$processingHome/app/resources/core/library")
val localCoreJar: File? = if (processingCoreDir.isDirectory) {
    processingCoreDir.listFiles()
        ?.filter { it.isFile && it.name.startsWith("core-") && it.name.endsWith(".jar") }
        ?.maxByOrNull { it.lastModified() }
} else {
    null
}

dependencies {
    if (localCoreJar != null) {
        // 本机 Processing core（推荐，和运行时完全一致）
        compileOnly(files(localCoreJar.absolutePath))
        println("cpplib: using local Processing core: ${localCoreJar.name}")
    } else {
        // 找不到本机 Processing，回退到 Maven 的 core（通常是 4.3.1 及以后都兼容）
        compileOnly(group = "org.processing", name = "core", version = "4.3.1")
    }

    // 你的其它 Java 依赖加在这里（一般 Java 侧只需要 core）
    // implementation(group = "org.apache.commons", name = "commons-math3", version = "3.6.1")

    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
}

tasks.test {
    useJUnitPlatform()
}

// ==========================
// END USER BUILD CONFIGURATIONS
// ==========================

// ----------------------------------------------------------------
// 原生库（C++/CMake）构建入口。产物输出到 <root>/native/<os>-<arch>/，
// 由 buildReleaseArtifacts 的 "Copy assets" 一并打进发布目录。
// 直接调用：gradlew buildNative
// ----------------------------------------------------------------
val nativeScript = if (currentOS.isWindows) "cpp\\build-native.bat" else "cpp/build-native.sh"
tasks.register<Exec>("buildNative") {
    group = "processing"
    description = "Build the native C++ library with CMake"
    workingDir = rootDir
    if (currentOS.isWindows) {
        commandLine("cmd", "/c", nativeScript, "windows-amd64")
    } else {
        commandLine("bash", nativeScript)
    }
}

// 一步到位：原生 + Java + 安装到 sketchbook
tasks.register("buildAndDeploy") {
    group = "processing"
    description = "Build native + Java, then install the library into the Processing sketchbook"
    dependsOn("buildNative", "deployToProcessingSketchbook")
}

// =============================
// INTERNAL BUILD CONFIGURATIONS
// 一般不要改动下面的内容
// =============================

tasks.jar {
    archiveBaseName.set(libName)
    archiveClassifier.set("")
    archiveVersion.set("")
}

tasks.test {
    useJUnitPlatform()
}

val releaseRoot = "$rootDir/release"
val releaseName = libName
val releaseDirectory = "$releaseRoot/$releaseName"

tasks.register<WriteProperties>("writeLibraryProperties") {
    group = "processing"
    destinationFile = project.file("library.properties")

    property("name", libraryProperties.getProperty("name"))
    property("version", libraryProperties.getProperty("version"))
    property("prettyVersion", project.version)
    property("authors", libraryProperties.getProperty("authors"))
    property("url", libraryProperties.getProperty("url"))
    property("categories", libraryProperties.getProperty("categories"))
    property("sentence", libraryProperties.getProperty("sentence"))
    property("paragraph", libraryProperties.getProperty("paragraph"))
    property("minRevision", libraryProperties.getProperty("minRevision"))
    property("maxRevision", libraryProperties.getProperty("maxRevision"))
}

tasks.build.get().mustRunAfter("clean")
tasks.javadoc.get().mustRunAfter("build")

tasks.register("buildReleaseArtifacts") {
    group = "processing"
    dependsOn("clean", "build", "javadoc", "writeLibraryProperties")
    finalizedBy("packageRelease", "duplicateZipToPdex")

    doFirst {
        println("Releasing library $libName")
        println(org.gradle.internal.jvm.Jvm.current())
        println("Cleaning release...")
        project.delete(files(releaseRoot))
    }

    doLast {
        println("Creating package...")

        println("Copy library...")
        copy {
            from(layout.buildDirectory.file("libs/${libName}.jar"))
            into("$releaseDirectory/library")
        }

        println("Copy dependencies...")
        copy {
            from(configurations.runtimeClasspath)
            into("$releaseDirectory/library")
        }

        println("Copy assets (native dll etc.)...")
        copy {
            from("$rootDir")
            include("shaders/**", "native/**")

            into("$releaseDirectory/library")
            exclude("*.DS_Store")
        }

        println("Copy javadoc...")
        copy {
            from(layout.buildDirectory.dir("docs/javadoc"))
            into("$releaseDirectory/reference")
        }

        println("Copy additional artifacts...")
        copy {
            from(rootDir)
            include("README.md", "readme/**", "library.properties", "examples/**", "src/**")

            into(releaseDirectory)
            exclude("*.DS_Store", "**/networks/**")
        }

        println("Copy repository library.txt...")
        copy {
            from(rootDir)
            include("library.properties")
            into(releaseRoot)
            rename("library.properties", "$libName.txt")
        }
    }
}

tasks.register<Zip>("packageRelease") {
    dependsOn("buildReleaseArtifacts")
    doFirst {
        println("Create zip file...")
    }
    archiveFileName.set("${libName}.zip")
    from(releaseDirectory)
    into(releaseName)
    destinationDirectory.set(file(releaseRoot))
    exclude("**/*.DS_Store")
}

tasks.register<Copy>("duplicateZipToPdex") {
    doFirst {
        println("Duplicate zip file to pdex extension...")
    }
    from(releaseRoot) {
        include("$libName.zip")
        rename("$libName.zip", "$libName.pdex")
    }
    into(releaseRoot)
}
tasks["duplicateZipToPdex"].mustRunAfter("packageRelease")

tasks.register("deployToProcessingSketchbook") {
    group = "processing"
    dependsOn("buildReleaseArtifacts")

    doFirst {
        println("Copy to sketchbook  $sketchbookLocation ...")
    }

    doLast {
        val installDirectory = file("$sketchbookLocation/libraries/$libName")

        println("Removing old install from: $installDirectory")
        delete(installDirectory)

        println("Copying fresh build to sketchbook $sketchbookLocation ...")
        copy {
            from(releaseDirectory)
            include(
                "library.properties",
                "examples/**",
                "library/**",
                "reference/**",
                "src/**"
            )
            into(installDirectory)
        }
    }
}
