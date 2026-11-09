<#
    Builds the Android APK and, optionally, installs and runs it.

    The game data (..\Build-Android\ZEQ2) goes inside the APK. The app
    unpacks it on its first run, and again only when the data changes.

    .\Build-Android.ps1                    Debug APK for arm64-v8a, copied to ..\Build-Android
    .\Build-Android.ps1 -Emulator          Also builds x86_64, for a PC emulator
    .\Build-Android.ps1 -Install           Installs it with adb
    .\Build-Android.ps1 -Run               Starts the game and follows the engine log
    .\Build-Android.ps1 -Run -Map desert   Starts straight into a map
    .\Build-Android.ps1 -Clean -Install    Uninstalls first: also deletes the unpacked data and config
    .\Build-Android.ps1 -Release           Release APK (optimized, no debugger): ..\Build-Android\ZEQ2-<version>.apk
    .\Build-Android.ps1 -Device <serial>   Picks the device when adb sees more than one (adb devices)

    The options can be combined: .\Build-Android.ps1 -Install -Run

    A release is signed with the key named in Android\keystore.properties
    (storeFile, storePassword, keyAlias, keyPassword; never commit it). Without
    that file it gets the debug key and "-debugkey" in its name: fine to
    test, but players could not update it to a properly signed APK.
#>
param(
    [switch]$Emulator,
    [switch]$Install,
    [switch]$Run,
    [switch]$Clean,
    [switch]$NoBuild,
    [switch]$Release,
    [string]$Device,
    [string]$Map
)

$ErrorActionPreference = 'Stop'

$package  = 'io.github.francofuster.zeq2remaster'
$launcher = "$package/.SetupActivity"
$source   = $PSScriptRoot
$output   = Join-Path (Split-Path $source -Parent) 'Build-Android'
$sdk      = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { 'D:\Android' }
$adb      = Join-Path $sdk 'platform-tools\adb.exe'
$apk      = Join-Path $output 'ZEQ2-debug.apk'
$adbArgs  = if ($Device) { @('-s', $Device) } else { @() }

if ($Release) {
    $props = Get-Content (Join-Path $source 'Android\gradle.properties')
    $version = (($props | Where-Object { $_ -match '^zeqVersionName=' }) -replace '^zeqVersionName=', '').Trim()
    $signed = Test-Path (Join-Path $source 'Android\keystore.properties')
    $apk = Join-Path $output ( 'ZEQ2-' + $version + $( if ($signed) { '' } else { '-debugkey' } ) + '.apk' )
}

function Adb {
    # adb reports progress on stderr. Windows PowerShell turns that into an
    # error when the output is redirected, so print it as plain text instead.
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $adb @adbArgs @args 2>&1 | ForEach-Object { "$_" }
    } finally {
        $ErrorActionPreference = $old
    }
    if ($LASTEXITCODE -ne 0) { throw "adb $args failed" }
}

if (-not $NoBuild) {
    if (-not $env:JAVA_HOME) { $env:JAVA_HOME = 'C:\Program Files\Java\jdk-27' }

    $gradleArgs = @( $( if ($Release) { ':app:assembleRelease' } else { ':app:assembleDebug' } ), '--console=plain')
    if ($Emulator) { $gradleArgs += '-PzeqAbis=arm64-v8a,x86_64' }

    Push-Location (Join-Path $source 'Android')
    try {
        & .\gradlew.bat @gradleArgs
        if ($LASTEXITCODE -ne 0) { throw 'Gradle failed' }
    } finally {
        Pop-Location
    }

    New-Item -ItemType Directory -Force $output | Out-Null
    if ($Release) {
        Copy-Item (Join-Path $source 'Android\app\build\outputs\apk\release\app-release.apk') $apk -Force
        if (-not $signed) {
            Write-Warning 'No Android\keystore.properties: signed with the debug key, for testing only.'
        }
    } else {
        Copy-Item (Join-Path $source 'Android\app\build\outputs\apk\debug\app-debug.apk') $apk -Force
    }
    Write-Host "APK: $apk"
}

if ($Clean) {
    # Ignore the error when it was not installed
    & $adb @adbArgs uninstall $package 2>&1 | ForEach-Object { "$_" }
}

if ($Install) {
    Adb install -r $apk
}

if ($Run) {
    Adb shell am force-stop $package
    Adb logcat -c
    if ($Map) {
        Adb shell am start -n $launcher --es args "`"+map $Map`""
    } else {
        Adb shell am start -n $launcher
    }
    # ZEQ2 is the engine console. The others show why a start fails.
    & $adb @adbArgs logcat -v brief -s ZEQ2:V SDL:V 'SDL/APP:V' libc:F DEBUG:V AndroidRuntime:E
}
