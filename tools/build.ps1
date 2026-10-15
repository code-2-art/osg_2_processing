# ============================================================
#  cpplib 一键构建 + 冒烟测试 + 安装到 Processing sketchbook
#
#  用法：
#     powershell -ExecutionPolicy Bypass -File tools\build.ps1
#
#  可选开关：
#     -Cuda            启用 CUDA 后端（需要本机 NVIDIA CUDA Toolkit）
#     -SkipNative      跳过 C++/CMake 构建（仅打包 Java + 组装）
#     -SkipTest        跳过 JNI 命令行冒烟测试
#     -SkipInstall     不安装到 Processing sketchbook
#     -JavaHome <path> 指定 JDK（默认使用 Processing 自带 JDK17）
#
#  产物：
#     build\cpplib.jar                  Java 侧 jar
#     native\windows-amd64\cpplib.dll   原生产物（由 CMake 输出/拷贝）
#     dist\cpplib\                      完整的可发布库目录
#     <sketchbook>\libraries\cpplib     安装结果（Processing 自动识别）
# ============================================================
[CmdletBinding()]
param(
    [switch]$Cuda,
    [switch]$SkipNative,
    [switch]$SkipTest,
    [switch]$SkipInstall,
    [string]$JavaHome = ""
)

$ErrorActionPreference = "Stop"
$Root    = Split-Path -Parent $PSScriptRoot
$LibName = "cpplib"

function Step($msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

# ---------- 1. 定位 Processing / JDK / core ----------
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
$javac = Join-Path $JavaHome "bin\javac.exe"
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

# ---------- 2. 编译 Java 并生成 JNI 头 ----------
$srcRoot    = Join-Path $Root "src\main\java"
$classesDir = Join-Path $Root "build\classes"
$hdrDir     = Join-Path $Root "native\include"
$jarFile    = Join-Path $Root "build\$LibName.jar"
New-Item -ItemType Directory -Force $classesDir, $hdrDir | Out-Null

Step "javac (regenerates native\include\org_example_cpplib_NativeCore.h)"
$sources = Get-ChildItem $srcRoot -Recurse -Filter *.java | ForEach-Object { $_.FullName }
& $javac -encoding UTF-8 -h $hdrDir -d $classesDir -classpath $coreJar.FullName $sources
if ($LASTEXITCODE -ne 0) { throw "javac failed" }

& $jarExe cf $jarFile -C $classesDir "."
if ($LASTEXITCODE -ne 0) { throw "jar failed" }
Write-Host "jar -> $jarFile"

$generatedHeader = Join-Path $hdrDir "org_example_cpplib_NativeCore.h"
if (-not (Test-Path $generatedHeader)) { throw "Expected header not generated: $generatedHeader" }

# ---------- 3. 构建 C++ / CMake ----------
if (-not $SkipNative) {
    Step "CMake native build  (cuda=$Cuda)"
    $nativeBat = Join-Path $Root "cpp\build-native.bat"
    $nativeArgs = @("windows-amd64")
    if ($JavaHome) { $nativeArgs += @("-java", $JavaHome) }
    if ($Cuda)     { $nativeArgs += "-cuda" }
    & $nativeBat @nativeArgs
    if ($LASTEXITCODE -ne 0) { throw "native build failed" }
} else {
    Write-Host "(native build skipped)"
}

# ---------- 4. 组装 dist\cpplib ----------
Step "Assemble dist\$LibName"
$dist = Join-Path $Root "dist\$LibName"
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
$distLibrary  = Join-Path $dist "library"
$distNative   = Join-Path $distLibrary "native"
$distExamples = Join-Path $dist "examples"
New-Item -ItemType Directory -Force $dist, $distLibrary, $distNative, $distExamples | Out-Null

Copy-Item (Join-Path $Root "library.properties") (Join-Path $dist "library.properties") -Force
Copy-Item $jarFile (Join-Path $distLibrary "$LibName.jar") -Force

# 原生产物（native\<os>-<arch>\ 子目录）
$rootNative = Join-Path $Root "native"
if (Test-Path $rootNative) {
    $subDirs = Get-ChildItem $rootNative -Directory
    foreach ($sub in $subDirs) {
        Copy-Item $sub.FullName (Join-Path $distNative $sub.Name) -Recurse -Force
    }
}

$examplesSrc = Join-Path $Root "examples"
if (Test-Path $examplesSrc) {
    Copy-Item "$examplesSrc\*" $distExamples -Recurse -Force
}

Write-Host "dist\cpplib contents:"
Get-ChildItem $dist -Recurse -File | ForEach-Object { Write-Host "  " $_.FullName.Substring($Root.Length) }

# ---------- 5. JNI 冒烟测试（不依赖 Processing 窗口） ----------
if (-not $SkipTest) {
    Step "JNI smoke test"
    $testSrcDir   = Join-Path $PSScriptRoot "jnitest"
    $testClasses  = Join-Path $Root "build\testclasses"
    New-Item -ItemType Directory -Force $testClasses | Out-Null
    $testSources  = Get-ChildItem $testSrcDir -Recurse -Filter *.java | ForEach-Object { $_.FullName }
    $testClassPath = (Join-Path $distLibrary "$LibName.jar")
    & $javac -encoding UTF-8 -d $testClasses -classpath $testClassPath $testSources
    if ($LASTEXITCODE -ne 0) { throw "test compile failed" }

    $runClassPath = "$testClassPath;$testClasses"
    & $javaExe -cp $runClassPath org.example.cpplib.TestMain
    if ($LASTEXITCODE -ne 0) { throw "JNI smoke test FAILED" }
    Write-Host "JNI smoke test PASSED" -ForegroundColor Green
}

# ---------- 6. 安装到 sketchbook ----------
if (-not $SkipInstall) {
    $sketchDoc = Join-Path $env:USERPROFILE "Documents\Processing"
    if (-not (Test-Path $sketchDoc)) {
        $sketchDoc = Join-Path $env:USERPROFILE "My Documents\Processing"
    }
    if (-not (Test-Path $sketchDoc)) {
        throw "Cannot find Processing sketchbook. Use -SkipInstall."
    }

    $librariesDir = Join-Path $sketchDoc "libraries"
    $dest = Join-Path $librariesDir $LibName
    Step "Install to sketchbook: $dest"
    New-Item -ItemType Directory -Force $librariesDir | Out-Null
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Copy-Item $dist $dest -Recurse -Force
    Write-Host "Installed. 请重启/重开 Processing，然后 File > Examples > Libraries 即可看到两个示例。"
} else {
    Write-Host ""
    Write-Host "Not installed. 发布目录在: $dist"
}
Write-Host ""
Write-Host "All done." -ForegroundColor Green
