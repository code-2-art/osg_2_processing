# ============================================================
#  osg_lib one-shot build + assembly + install into Processing (Windows)
#
#  Usage:
#     powershell -ExecutionPolicy Bypass -File osg_lib\build.ps1
#
#  Optional switches:
#     -SkipNative      skip the C++/CMake build (only package Java + assemble)
#     -SkipInstall     do not install into the Processing sketchbook
#     -SkipTest        skip the JNI load smoke test
#     -JavaHome <path> JDK to use (defaults to Processing's bundled JDK 17)
#     -OsgSdk <path>   OSG SDK path (defaults to $DefaultOsgSdk below)
#
#  Outputs:
#     osg_lib\build\osglib.jar                       Java jar
#     osg_lib\native\windows-amd64\osglib.dll        native artifact + copied OSG runtime
#     osg_lib\dist\osglib\                          complete publishable directory
#     <sketchbook>\libraries\osglib                 installed result
# ============================================================
[CmdletBinding()]
param(
    [switch]$SkipNative,
    [switch]$SkipInstall,
    [switch]$SkipTest,
    [string]$JavaHome = "",
    [string]$OsgSdk = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$Root     = Split-Path -Parent $ScriptDir
$LibName  = "osglib"

# Default OSG SDK (matches the path the user provided in README; override with -OsgSdk)
$DefaultOsgSdk = "D:\Projects\osgVerse\osg-build\core_x64\sdk"

function Step($msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

# ---------- 1. Locate Processing / JDK / core ----------
$procHome = $env:PROCESSING_HOME
if (-not $procHome) {
    $cand = @()
    if ($env:ProgramFiles)        { $cand += "$env:ProgramFiles\Processing" }
    if ($env:ProgramW6432)        { $cand += "$env:ProgramW6432\Processing" }
    if (${env:ProgramFiles(x86)}) { $cand += "${env:ProgramFiles(x86)}\Processing" }
    foreach ($c in $cand) {
        if (Test-Path $c) { $procHome = $c; break }
    }
}
if (-not $procHome) { throw "Cannot find Processing. Set PROCESSING_HOME." }

if (-not $JavaHome) { $JavaHome = Join-Path $procHome "app\resources\jdk" }
$javac  = Join-Path $JavaHome "bin\javac.exe"
$jarExe = Join-Path $JavaHome "bin\jar.exe"
$javaExe = Join-Path $JavaHome "bin\java.exe"
foreach ($tool in @($javac, $jarExe, $javaExe)) {
    if (-not (Test-Path $tool)) { throw "JDK tool missing: $tool (pass -JavaHome)" }
}
Step "Using JDK: $JavaHome"

$coreDir = Join-Path $procHome "app\resources\core\library"
$coreJar = Get-ChildItem $coreDir -Filter "core-*.jar" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $coreJar) { throw "Processing core jar not found under $coreDir" }
Step "Processing core: $($coreJar.FullName)"

# OSG SDK
if (-not $OsgSdk) { $OsgSdk = $env:OSG_SDK_ROOT }
if (-not $OsgSdk) { $OsgSdk = $DefaultOsgSdk }
if (-not (Test-Path "$OsgSdk\include\osgViewer\Viewer")) {
    throw "OSG SDK not found at '$OsgSdk' (need include/osgViewer/Viewer). Pass -OsgSdk."
}
Step "OSG SDK: $OsgSdk"

# GL header directory (a GLCORE build of OSG ships no include/GL, extra GL headers are needed)
$glInclude = $env:OSG_GL_INCLUDE_DIR
if (-not $glInclude) {
    $glCands = @(
        "D:\Projects\osgVerse\master\helpers\toolchain_builder\opengl",
        "D:\Projects\osgVerse\master\3rdparty"
    )
    foreach ($c in $glCands) {
        if (Test-Path "$c\GL\glcorearb.h") { $glInclude = $c; break }
    }
}
if (-not $glInclude) {
    throw "GL headers (GL/glcorearb.h) not found. Set OSG_GL_INCLUDE_DIR to the dir containing GL/."
}
Step "GL headers: $glInclude"

# ---------- 2. Compile Java and generate the JNI header ----------
$srcRoot    = Join-Path $ScriptDir "java"
$classesDir = Join-Path $ScriptDir "build\classes"
$hdrDir     = Join-Path $ScriptDir "native\include"
$jarFile    = Join-Path $ScriptDir "build\$LibName.jar"
New-Item -ItemType Directory -Force $classesDir, $hdrDir | Out-Null

Step "javac (regenerates native\include\org_example_osglib_NativeOsgCore.h)"
$sources = Get-ChildItem $srcRoot -Recurse -Filter *.java | ForEach-Object { $_.FullName }
& $javac -encoding UTF-8 -h $hdrDir -d $classesDir -classpath $coreJar.FullName $sources
if ($LASTEXITCODE -ne 0) { throw "javac failed" }

& $jarExe cf $jarFile -C $classesDir "."
if ($LASTEXITCODE -ne 0) { throw "jar failed" }
Write-Host "jar -> $jarFile"

$generatedHeader = Join-Path $hdrDir "org_example_osglib_NativeOsgCore.h"
if (-not (Test-Path $generatedHeader)) { throw "Expected header not generated: $generatedHeader" }

# ---------- 3. Build C++ / CMake ----------
if (-not $SkipNative) {
    Step "CMake native build"
    $buildDir = Join-Path $ScriptDir "build\windows-amd64"
    $outDir   = Join-Path $ScriptDir "native\windows-amd64"
    New-Item -ItemType Directory -Force $outDir | Out-Null

    $cmakeArgs = @(
        "-S", $ScriptDir,
        "-B", $buildDir,
        "-A", "x64",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCPPLIB_OUTPUT_DIR=$outDir",
        "-DCPPLIB_JAVA_HOME=$JavaHome",
        "-DOSG_SDK_ROOT=$OsgSdk",
        "-DOSG_GL_INCLUDE_DIR=$glInclude"
    )
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

    & cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

    # A multi-config generator may drop the artifact under <outDir>/Release; fall back to
    # a recursive search and copy it up to <outDir>.
    if (-not (Test-Path "$outDir\osglib.dll")) {
        $candidates = Get-ChildItem "$outDir" -Recurse -Filter "osglib.dll" -ErrorAction SilentlyContinue
        if ($candidates.Count -gt 0) { Copy-Item $candidates[0].FullName $outDir -Force }
    }
    if (-not (Test-Path "$outDir\osglib.dll")) { throw "osglib.dll not produced" }
    Write-Host "artifact: $outDir\osglib.dll"

    # ---------- 3b. Copy OSG runtime DLLs into the native dir so they resolve at load time ----------
    Step "Copy OSG runtime DLLs"
    $osgBin = Join-Path $OsgSdk "bin"
    # Core libs: the Release builds of osg202-* (no trailing "d") plus ot21-OpenThreads.dll
    $coreDlls = Get-ChildItem $osgBin -Filter "osg202-*.dll" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch 'd\.dll$' }
    $coreDlls += Get-ChildItem $osgBin -Filter "ot21-OpenThreads.dll" -ErrorAction SilentlyContinue
    foreach ($d in $coreDlls) { Copy-Item $d.FullName $outDir -Force }
    # Plugin dir (osgPlugins-3.7.0): copy only the common, lightweight Release plugins
    # (exclude *d.dll debug builds). NOTE: osgDB scans and loads every plugin in this dir at
    # init, so we must never pull in plugins that depend on heavy runtimes (ffmpeg / web etc.)
    # or debug builds, or the whole load fails for lack of dependencies.
    $pluginSrc = Join-Path $osgBin "osgPlugins-3.7.0"
    $pluginDst = Join-Path $outDir "osgPlugins-3.7.0"
    $essentialPlugins = @(
        "osgdb_osg", "osgdb_osgb", "osgdb_osgt", "osgdb_osgx",
        "osgdb_obj", "osgdb_ive", "osgdb_stl", "osgdb_3ds",
        "osgdb_dds", "osgdb_bmp", "osgdb_jpeg", "osgdb_png",
        "osgdb_tga", "osgdb_tiff", "osgdb_zip", "osgdb_pnm", "osgdb_rgb"
    )
    if (Test-Path $pluginSrc) {
        New-Item -ItemType Directory -Force $pluginDst | Out-Null
        foreach ($p in $essentialPlugins) {
            $src = Join-Path $pluginSrc "$p.dll"
            if (Test-Path $src) { Copy-Item $src $pluginDst -Force }
        }
    }
    Write-Host "Copied $(($coreDlls | Measure-Object).Count) core DLLs + $(($essentialPlugins | Measure-Object).Count) plugins into $outDir"

    # VC++ runtime (MSVCP140 / VCRUNTIME140 / VCRUNTIME140_1): the copies in this machine's
    # System32 / VS redist dirs are treated by the OS loader as bad images (LoadLibrary returns
    # 203), which makes osglib fail to load entirely. Processing's bundled JDK ships a *working*
    # runtime in its bin (Java itself uses it), so we copy from there into the same dir as osglib.
    # On a target machine without the VC runtime, install "Visual C++ Redistributable" first.
    foreach ($n in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
        $p = Join-Path $JavaHome "bin\$n"
        if (Test-Path $p) { Copy-Item $p $outDir -Force }
    }
    Write-Host "Copied VC runtime from JDK bin ($JavaHome\bin)"

    # 3rd-party deps (zlib, freetype, etc.): loaded by OSG modules at runtime, must be copied too.
    # Only copy DLLs actually depended on by the OSG core, to avoid dragging in unrelated heavy
    # Dependencies DLLs.
    $depsBin = "D:\Projects\osgVerse\Dependencies\x64\msvc18\bin"
    $depNames = @("z.dll", "freetype.dll", "libpng16.dll", "bz2.dll",
                  "jpeg62.dll", "tiff.dll", "libwebp.dll", "libwebpdecoder.dll",
                  "brotlidec.dll", "brotlicommon.dll")
    if (Test-Path $depsBin) {
        foreach ($n in $depNames) {
            $p = Join-Path $depsBin $n
            if (Test-Path $p) { Copy-Item $p $outDir -Force }
        }
        Write-Host "Copied 3rdparty runtime DLLs from $depsBin"
    } else {
        Write-Warning "Dependencies bin not found at $depsBin; zlib/freetype etc. may be missing."
    }

    # tiff.dll needs liblzma.dll and libwebp.dll needs libsharpyuv.dll, but those two only ship as
    # .lib (no .dll) inside the OSG Dependencies, so they are absent from the OSG Deps bin. They are
    # provided by vcpkg's x64-windows-release (shares VCRUNTIME140 with the VS2017-built OSG, ABI
    # compatible).
    $vcpkgBin = "D:\Projects\CommonDeps\vcpkg\installed\x64-windows-release\bin"
    $vcpkgDeps = @("liblzma.dll", "libsharpyuv.dll")
    if (Test-Path $vcpkgBin) {
        foreach ($n in $vcpkgDeps) {
            $p = Join-Path $vcpkgBin $n
            if (Test-Path $p) { Copy-Item $p $outDir -Force }
        }
        Write-Host "Copied $($vcpkgDeps -join ', ') from $vcpkgBin"
    } else {
        Write-Warning "vcpkg bin not found at $vcpkgBin; liblzma/libsharpyuv may be missing (tiff/webp will fail)."
    }
} else {
    Write-Host "(native build skipped)"
}

# ---------- 4. Assemble dist\osglib ----------
Step "Assemble dist\$LibName"
$dist = Join-Path $ScriptDir "dist\$LibName"
$distLibrary  = Join-Path $dist "library"
$distNative   = Join-Path $distLibrary "native"
$distExamples = Join-Path $dist "examples"
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist, $distLibrary, $distNative, $distExamples | Out-Null

Copy-Item (Join-Path $ScriptDir "library.properties") (Join-Path $dist "library.properties") -Force
Copy-Item $jarFile (Join-Path $distLibrary "$LibName.jar") -Force

$rootNative = Join-Path $ScriptDir "native"
if (Test-Path $rootNative) {
    $subDirs = Get-ChildItem $rootNative -Directory
    foreach ($sub in $subDirs) {
        Copy-Item $sub.FullName (Join-Path $distNative $sub.Name) -Recurse -Force
    }
}

# Only package the examples that belong to osglib
if (Test-Path $Root) {
    $osglibExamples = @("CppOsgViewer")
    foreach ($ex in $osglibExamples) {
        $exSrc = Join-Path $Root "examples\$ex"
        if (Test-Path $exSrc) { Copy-Item $exSrc (Join-Path $distExamples $ex) -Recurse -Force }
    }
}

Write-Host "dist\$LibName contents:"
Get-ChildItem $dist -Recurse -File | ForEach-Object { Write-Host "  " $_.FullName.Substring($ScriptDir.Length) }

# ---------- 5. JNI load smoke test (verify the .dll and its OSG deps all load) ----------
if (-not $SkipTest) {
    Step "JNI load smoke test"
    $testDir = Join-Path $ScriptDir "build\jnitest"
    New-Item -ItemType Directory -Force $testDir | Out-Null
    $testFile = Join-Path $testDir "OsgLoadTest.java"
    $testSrc = @(
        "public class OsgLoadTest {",
        "    public static void main(String[] args) {",
        "        System.loadLibrary(""osglib"");",
        "        System.out.println(""osglib native library loaded OK"");",
        "    }",
        "}"
    ) -join "`n"
    Set-Content -Path $testFile -Value $testSrc -Encoding ASCII
    & $javac -encoding UTF-8 -d $testDir -classpath "$distLibrary\$LibName.jar" $testFile
    if ($LASTEXITCODE -ne 0) { throw "test compile failed" }
    # Let the JVM prefer loading from the native dir (consistent with Processing's behavior)
    $nativeDir = Join-Path $ScriptDir "native\windows-amd64"
    # At runtime OSG's init loads plugins via the relative path "osgPlugins-3.7.0\*.dll", and the
    # Windows loader only searches CWD and PATH for relative paths (not the calling DLL's own
    # subdir). So the native dir MUST be on the [process PATH]; otherwise osgPlugins-3.7.0 is not
    # found. The native dir already carries all 3rd-party deps and the VC runtime (from JDK bin),
    # so we only need to add the native dir and JDK bin to PATH.
    $env:PATH = "$nativeDir;$JavaHome\bin;$env:PATH"
    & $javaExe "-Djava.library.path=$nativeDir" -cp "$distLibrary\$LibName.jar;$testDir" OsgLoadTest
    if ($LASTEXITCODE -ne 0) { throw "JNI load smoke test FAILED" }
    Write-Host "JNI load smoke test PASSED" -ForegroundColor Green
}

# ---------- 6. Install into the sketchbook ----------
if (-not $SkipInstall) {
    # Collect candidate sketchbook roots:
    #   - every sketchbook.path* key from Processing's preferences.txt (the actually-used sketchbook
    #     can be any of these, e.g. a custom path like D:/Processing)
    #   - the default Documents / My Documents locations as a fallback
    $sketchRoots = @()
    $prefFile = Join-Path $env:APPDATA "Processing\preferences.txt"
    if (Test-Path $prefFile) {
        Get-Content $prefFile -Encoding UTF8 | ForEach-Object {
            if ($_ -match '^sketchbook\.path(\.\w+)?=(.*)$') {
                $sketchRoots += $Matches[2].Trim()
            }
        }
    }
    $sketchRoots += (Join-Path $env:USERPROFILE "Documents\Processing")
    $sketchRoots += (Join-Path $env:USERPROFILE "My Documents\Processing")

    $installed = $false
    foreach ($sb in ($sketchRoots | Select-Object -Unique)) {
        if (-not $sb) { continue }
        $libDir = Join-Path $sb "libraries"
        New-Item -ItemType Directory -Force $libDir | Out-Null
        $dest = Join-Path $libDir $LibName
        Step "Install to sketchbook: $dest"
        if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
        Copy-Item $dist $dest -Recurse -Force
        $installed = $true
    }
    if (-not $installed) { throw "Cannot find a Processing sketchbook to install into. Use -SkipInstall." }
    Write-Host "Installed. Restart/reopen Processing, then File > Examples > Libraries to see the examples."
} else {
    Write-Host ""
    Write-Host "Not installed. Publish directory at: $dist"
}
Write-Host ""
Write-Host "All done." -ForegroundColor Green
