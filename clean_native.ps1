# Clean rebuild helper: remove the native/ and dist/ output of both libraries so the next
# build.ps1 run regenerates everything from scratch.
$base = 'd:\Projects\Misc\AnyTest\osg_2_processing'
Remove-Item -Recurse -Force (Join-Path $base 'osg_lib\native\windows-amd64') -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force (Join-Path $base 'osg_lib\dist') -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force (Join-Path $base 'osgverse_lib\native\windows-amd64') -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force (Join-Path $base 'osgverse_lib\dist') -ErrorAction SilentlyContinue
Write-Host "cleaned native/dist for both libs"
