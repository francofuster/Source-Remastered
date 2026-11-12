<#
    Builds the Android APK and, optionally, installs and runs it, or stages
    and publishes a release that the app updates itself from.

    The game data (..\Build-Android\ZEQ2) is packed as zeq2data.zip. A debug
    APK carries it inside and unpacks it on its first run. A release APK does
    not: the zip goes to the GitHub release next to the APK and a
    manifest.json, and the app downloads the data the first time and then
    only the files that changed (SetupActivity, Updater).

    .\Build-Android.ps1                    Debug APK for arm64-v8a and armeabi-v7a, data inside, copied to ..\Build-Android
    .\Build-Android.ps1 -Emulator          arm64-v8a and x86_64 instead, for a PC emulator
    .\Build-Android.ps1 -Install           Installs it with adb
    .\Build-Android.ps1 -Install -Abi armeabi-v7a
                                          Installs the 32-bit code on a 64-bit phone, to test it
    .\Build-Android.ps1 -Run               Starts the game and follows the engine log
    .\Build-Android.ps1 -Run -Map desert   Starts straight into a map
    .\Build-Android.ps1 -Clean -Install    Uninstalls first: also deletes the unpacked data and config
    .\Build-Android.ps1 -Release           Release APK, no data inside: ..\Build-Android\ZEQ2-Lite-Mobile-<version>.apk
    .\Build-Android.ps1 -Device <serial>   Picks the device when adb sees more than one (adb devices)

    Releases (the APK, zeq2data.zip and manifest.json in ..\Build-Android\release\<tag>):
    .\Build-Android.ps1 -Stage -Notes "Short text the update dialog shows"
                                          Builds the release APK and stages the three files, tag android-<version>
    .\Build-Android.ps1 -Stage -DataOnly -Tag android-1.4-alpha3-d2 -Notes "..."
                                          Only the data changed: reuses the APK already built for this version
    .\Build-Android.ps1 -Publish -NotesFile notes.md -Notes "..."
                                          Stages, then creates the GitHub prerelease on the current commit
                                          (pushed). -Replace deletes a release with the same tag first.
    .\Build-Android.ps1 -Run -Feed http://localhost:8000/releases
                                          Starts the app against a test release server (Android\tools\update_server.py)

    -VersionName and -VersionCode override Android\gradle.properties, for test
    builds. -DataNeeds sets the lowest versionCode that can use the staged
    data (default: the staged APK's own). -BundleData true|false overrides
    whether the APK carries the data. -Abis picks the ABIs (for example x86,
    for the Android 7 emulator).

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
    [switch]$Stage,
    [switch]$Publish,
    [switch]$DataOnly,
    [switch]$Replace,
    [string]$Tag,
    [string]$Notes = '',
    [string]$NotesFile,
    [string]$VersionName,
    [long]$VersionCode = 0,
    [long]$DataNeeds = 0,
    [string]$BundleData,
    [string]$Abis,
    [string]$Feed,
    [string]$Device,
    [string]$Abi,
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
$packed   = Join-Path $source 'Android\app\build\generated\assets\packZeqData'

if ($Publish) { $Stage = $true }
if ($Stage) { $Release = $true }

$props = Get-Content (Join-Path $source 'Android\gradle.properties')
$version = if ($VersionName) { $VersionName } else {
    (($props | Where-Object { $_ -match '^zeqVersionName=' }) -replace '^zeqVersionName=', '').Trim()
}
$signed = Test-Path (Join-Path $source 'Android\keystore.properties')
if ($Release) {
    $apk = Join-Path $output ( 'ZEQ2-Lite-Mobile-' + $version + $( if ($signed) { '' } else { '-debugkey' } ) + '.apk' )
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

function Gradle([string[]]$tasks) {
    if (-not $env:JAVA_HOME) { $env:JAVA_HOME = 'C:\Program Files\Java\jdk-27' }
    $gradleArgs = $tasks + @('--console=plain')
    if ($Abis) { $gradleArgs += "-PzeqAbis=$Abis" }
    elseif ($Emulator) { $gradleArgs += '-PzeqAbis=arm64-v8a,x86_64' }
    if ($VersionName) { $gradleArgs += "-PzeqVersionName=$VersionName" }
    if ($VersionCode -gt 0) { $gradleArgs += "-PzeqVersionCode=$VersionCode" }
    if ($BundleData) { $gradleArgs += "-PzeqBundleData=$BundleData" }
    Push-Location (Join-Path $source 'Android')
    # javac and CMake print notes on stderr. Windows PowerShell turns those
    # into errors when the output is redirected: print them as plain text and
    # trust the exit code instead.
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & .\gradlew.bat @gradleArgs 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { throw 'Gradle failed' }
    } finally {
        $ErrorActionPreference = $old
        Pop-Location
    }
}

if (-not $NoBuild) {
    New-Item -ItemType Directory -Force $output | Out-Null
    if ($DataOnly) {
        # Same code as the last release of this version: only pack the data
        Gradle @(':app:packZeqData')
        if (-not (Test-Path $apk)) { throw "-DataOnly reuses $apk, which is not there: build it with -Release first" }
    } else {
        Gradle @( $( if ($Release) { ':app:assembleRelease' } else { ':app:assembleDebug' } ) )
        if ($Release) {
            Copy-Item (Join-Path $source 'Android\app\build\outputs\apk\release\app-release.apk') $apk -Force
            if (-not $signed) {
                Write-Warning 'No Android\keystore.properties: signed with the debug key, for testing only.'
            }
        } else {
            Copy-Item (Join-Path $source 'Android\app\build\outputs\apk\debug\app-debug.apk') $apk -Force
        }
    }
    Write-Host "APK: $apk"
}

if ($Stage) {
    if (-not (Test-Path $apk)) { throw "No APK to stage: $apk" }
    $zip = Join-Path $packed 'zeq2data.zip'
    $idFile = Join-Path $packed 'zeq2data.id'
    if (-not (Test-Path $zip)) { throw "No packed data: $zip" }

    # versionCode and versionName as the APK itself says, not as assumed
    $aapt = Get-ChildItem (Join-Path $sdk 'build-tools') -Directory | Sort-Object Name | Select-Object -Last 1
    $badging = & (Join-Path $aapt.FullName 'aapt2.exe') dump badging $apk 2>$null | Select-Object -First 1
    if ($badging -notmatch "versionCode='(\d+)' versionName='([^']+)'") { throw "Cannot read the version of $apk" }
    $apkCode = [long]$Matches[1]
    $apkName = $Matches[2]

    if (-not $Tag) { $Tag = "android-$apkName" }
    if (-not $Tag.StartsWith('android-')) { throw "The tag must start with android- (the app only looks at those): $Tag" }
    $stageDir = Join-Path $output "release\$Tag"
    if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
    New-Item -ItemType Directory -Force $stageDir | Out-Null

    $apkFile = 'ZEQ2-Lite-Mobile-' + $apkName + '.apk'
    Copy-Item $apk (Join-Path $stageDir $apkFile)
    Copy-Item $zip (Join-Path $stageDir 'zeq2data.zip')
    $id = Get-Content $idFile

    $manifest = [ordered]@{
        version     = $apkName
        versionCode = $apkCode
        apk         = [ordered]@{
            file   = $apkFile
            bytes  = (Get-Item $apk).Length
            sha256 = (Get-FileHash $apk -Algorithm SHA256).Hash.ToLower()
        }
        data        = [ordered]@{
            file             = 'zeq2data.zip'
            id               = $id[0].Trim()
            bytes            = (Get-Item $zip).Length
            unpackedBytes    = [long]$id[1].Trim()
            files            = [int]$id[2].Trim()
            needsVersionCode = $( if ($DataNeeds -gt 0) { $DataNeeds } else { $apkCode } )
        }
        notes       = $Notes
    }
    $json = $manifest | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText((Join-Path $stageDir 'manifest.json'), $json, (New-Object Text.UTF8Encoding $false))
    Write-Host "Staged $Tag in ${stageDir}:"
    Get-ChildItem $stageDir | ForEach-Object { Write-Host ('  {0,-40} {1,12:N0} bytes' -f $_.Name, $_.Length) }
}

if ($Publish) {
    if (-not $signed) { throw 'Refusing to publish an APK signed with the debug key' }
    if (-not $NotesFile -or -not (Test-Path $NotesFile)) { throw '-Publish needs -NotesFile with the release notes' }
    $sha = (git -C $source rev-parse HEAD).Trim()
    $remote = git -C $source branch -r --contains $sha
    if (-not $remote) { throw "Commit $sha is not pushed: push it before publishing" }

    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & gh release view $Tag -R francofuster/Source-Remastered *> $null
    $exists = $LASTEXITCODE -eq 0
    $ErrorActionPreference = $old
    if ($exists) {
        if (-not $Replace) { throw "Release $Tag exists: pass -Replace to delete and recreate it, or another -Tag" }
        gh release delete $Tag -R francofuster/Source-Remastered --cleanup-tag --yes
        if ($LASTEXITCODE -ne 0) { throw 'gh release delete failed' }
    }
    $files = Get-ChildItem $stageDir | ForEach-Object { $_.FullName }
    gh release create $Tag @files -R francofuster/Source-Remastered --target $sha --prerelease --latest=false `
        --title "ZEQ2-Lite Mobile $apkName (Android)" --notes-file $NotesFile
    if ($LASTEXITCODE -ne 0) { throw 'gh release create failed' }
}

if ($Clean) {
    # Ignore the error when it was not installed
    & $adb @adbArgs uninstall $package 2>&1 | ForEach-Object { "$_" }
}

if ($Install) {
    if ($Abi) {
        # A phone that runs both picks the 64-bit code: this forces the other
        Adb install -r --abi $Abi $apk
    } else {
        Adb install -r $apk
    }
}

if ($Run) {
    Adb shell am force-stop $package
    Adb logcat -c
    $extras = @()
    if ($Map) { $extras += @('--es', 'args', "`"+map $Map`"") }
    if ($Feed) { $extras += @('--es', 'updateFeed', $Feed) }
    Adb shell am start -n $launcher @extras
    # ZEQ2 is the engine console. The others show why a start fails.
    & $adb @adbArgs logcat -v brief -s ZEQ2:V SDL:V 'SDL/APP:V' libc:F DEBUG:V AndroidRuntime:E
}
