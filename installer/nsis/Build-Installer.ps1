param(
    [string]$QtBin = "C:\Qt\6.11.2\mingw_64\bin",
    [string]$MingwBin = "C:\Qt\Tools\mingw1310_64\bin",
    [string]$Nsis = "C:\Program Files (x86)\NSIS\Bin\makensis.exe",
    [string]$Version = "0.3.0"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$buildDir = Join-Path $repoRoot "build-modern"
$releaseDir = Join-Path $buildDir "release"
$distDir = Join-Path $repoRoot "dist"
$stageRoot = Join-Path $distDir "stage"
$stageDir = Join-Path $stageRoot "Proxmark3GUI Modern"
$projectFile = Join-Path $repoRoot "src\Proxmark3GUI.pro"
$installerScript = Join-Path $PSScriptRoot "Proxmark3GUI-Modern.nsi"
$outFile = Join-Path $distDir "Proxmark3GUI-Modern-$Version-setup.exe"

if(!(Test-Path -LiteralPath (Join-Path $QtBin "qmake.exe"))) {
    throw "qmake.exe was not found in $QtBin"
}
if(!(Test-Path -LiteralPath (Join-Path $QtBin "windeployqt.exe"))) {
    throw "windeployqt.exe was not found in $QtBin"
}
if(!(Test-Path -LiteralPath (Join-Path $MingwBin "mingw32-make.exe"))) {
    throw "mingw32-make.exe was not found in $MingwBin"
}
if(!(Test-Path -LiteralPath $Nsis)) {
    throw "makensis.exe was not found at $Nsis"
}

$env:PATH = "$QtBin;$MingwBin;$env:PATH"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir
try {
    & (Join-Path $QtBin "qmake.exe") $projectFile
    & (Join-Path $MingwBin "mingw32-make.exe") -j4
}
finally {
    Pop-Location
}

$exePath = Join-Path $releaseDir "Proxmark3GUI.exe"
if(!(Test-Path -LiteralPath $exePath)) {
    throw "Build did not produce $exePath"
}

& (Join-Path $QtBin "windeployqt.exe") $exePath

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
if(Test-Path -LiteralPath $stageRoot) {
    $resolvedStage = Resolve-Path -LiteralPath $stageRoot
    if(-not $resolvedStage.Path.StartsWith((Resolve-Path -LiteralPath $distDir).Path, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected stage path: $resolvedStage"
    }
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

$runtimeFiles = @(
    "Proxmark3GUI.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Network.dll",
    "Qt6SerialPort.dll",
    "Qt6Svg.dll",
    "Qt6Widgets.dll",
    "D3Dcompiler_47.dll",
    "opengl32sw.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

foreach($file in $runtimeFiles) {
    $source = Join-Path $releaseDir $file
    if(Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $stageDir -Force
    }
}

$runtimeDirs = @(
    "generic",
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "styles",
    "tls",
    "translations"
)

foreach($dir in $runtimeDirs) {
    $source = Join-Path $releaseDir $dir
    if(Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $stageDir -Recurse -Force
    }
}

Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $stageDir -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stageDir -Force

& $Nsis "/DAPP_VERSION=$Version" "/DSTAGE_DIR=$stageDir" "/DOUT_FILE=$outFile" $installerScript

Write-Host "Installer created: $outFile"
