# Launcher: add the osg native dir to the [process PATH] before starting Processing.
# Reason: at init OSG loads plugins via the relative path "osgPlugins-3.7.0\*.dll", and the
# Windows loader only searches CWD and PATH for relative paths (not the calling DLL's own subdir),
# so the native dir must be on the process PATH for osglib/osgverselib to load.
param(
    [string]$Sketch = "",
    [string]$ProcessingExe = "C:\Program Files\Processing\Processing.exe"
)

$root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$nativeDir       = Join-Path $root "osg_lib\native\windows-amd64"
$osgverseNative = Join-Path $root "osgverse_lib\native\windows-amd64"
# Processing actually loads osglib.dll/osgverselib.dll from the sketchbook-installed library dir,
# so the native dir needed for OSG plugin relative-path loading must be on PATH too.
$sketchDoc = Join-Path $env:USERPROFILE "Documents\Processing"
if (-not (Test-Path $sketchDoc)) {
    $sketchDoc = Join-Path $env:USERPROFILE "My Documents\Processing"
}
$installedNative        = Join-Path $sketchDoc "libraries\osglib\library\native\windows-amd64"
$installedOsgverseNative = Join-Path $sketchDoc "libraries\osgverselib\library\native\windows-amd64"
$depBin    = "D:\Projects\osgVerse\Dependencies\x64\msvc18\bin"
$jdkBin    = "C:\Program Files\Processing\app\resources\jdk\bin"

if (-not (Test-Path $nativeDir)) {
    Write-Error "native dir not found: $nativeDir`nRun osg_lib\build.ps1 first."
    exit 1
}

# Both native dirs ship the same OSG core version, so adding both to PATH is conflict-free and
# covers both example types: CppOsgViewer (osglib) and CppOsgVerseViewer/Earth (osgverselib + osgVerse plugins).
function Add-NativeIfExists($p) { if (Test-Path $p) { "$p;" } else { "" } }
$pre = (Add-NativeIfExists $nativeDir) +
       (Add-NativeIfExists $osgverseNative) +
       (Add-NativeIfExists $installedNative) +
       (Add-NativeIfExists $installedOsgverseNative)
$env:PATH = "$pre$depBin;$jdkBin;$env:PATH"
# Also tell OSG the plugin search roots (lazy-loaded plugins resolve by absolute path, extra safety).
$env:OSG_LIBRARY_PATH = "$nativeDir;$osgverseNative;$installedNative;$installedOsgverseNative"

Write-Host "PATH now includes: $nativeDir"

if (Test-Path $ProcessingExe) {
    Write-Host "Starting Processing: $ProcessingExe"
    if ($Sketch -ne "") {
        Start-Process -FilePath $ProcessingExe -ArgumentList $Sketch
    } else {
        Start-Process -FilePath $ProcessingExe
    }
} else {
    Write-Error "Processing not found: $ProcessingExe"
    exit 1
}
