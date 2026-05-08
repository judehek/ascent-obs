param(
    [string]$TargetPath = "",
    [string]$Configuration = "RelWithDebInfo",
    [switch]$DryRun
)

# --- Configuration ---
$ObsStudioDir = Join-Path $PSScriptRoot "..\obs-studio"
$ObsStudioDir = (Resolve-Path $ObsStudioDir -ErrorAction Stop).Path

if ($TargetPath -eq "") {
    $TargetPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "ascent-obs"
}

$AscentObsDir = Join-Path $PSScriptRoot "..\ascent-obs"
$AscentObsDir = (Resolve-Path $AscentObsDir -ErrorAction Stop).Path
$DepsDir = Join-Path $ObsStudioDir ".deps\obs-deps-2024-03-19-x64"
$BuildDir = Join-Path $ObsStudioDir "build_x64"
$RunDir = Join-Path $BuildDir "rundir\$Configuration"

$SuccessCount = 0
$SkipCount = 0
$ErrorCount = 0

Write-Host "======================================================="
Write-Host "  Ascent-OBS Build Collector"
Write-Host "======================================================="
Write-Host "OBS Studio:    $ObsStudioDir"
Write-Host "Build Config:  $Configuration"
Write-Host "Target:        $TargetPath"
if ($DryRun) { Write-Host "MODE:          DRY RUN (no files will be copied)" -ForegroundColor Yellow }
Write-Host "======================================================="

# --- Helper: Copy a single file ---
function Copy-BuildFile {
    param(
        [string]$SourcePath,
        [string]$DestRelDir,
        [string]$DestFileName = ""
    )

    $FileName = if ($DestFileName -ne "") { $DestFileName } else { Split-Path $SourcePath -Leaf }
    $DestDir = Join-Path $TargetPath $DestRelDir
    $DestFile = Join-Path $DestDir $FileName

    if (-not (Test-Path $SourcePath -PathType Leaf)) {
        Write-Warning "  SKIP: $SourcePath (not found)"
        $script:SkipCount++
        return
    }

    if ($DryRun) {
        Write-Host "  [DRY] $FileName -> $DestRelDir" -ForegroundColor Cyan
        $script:SuccessCount++
        return
    }

    try {
        if (-not (Test-Path $DestDir)) {
            New-Item -Path $DestDir -ItemType Directory -Force -ErrorAction Stop | Out-Null
        }
        Copy-Item -Path $SourcePath -Destination $DestFile -Force -ErrorAction Stop
        Write-Host "  OK: $FileName -> $DestRelDir" -ForegroundColor Green
        $script:SuccessCount++
    } catch {
        Write-Error "  FAIL: $FileName -> $DestRelDir : $($_.Exception.Message)"
        $script:ErrorCount++
    }
}

# =======================================================
# 1. Ascent-OBS executable
# =======================================================
Write-Host "`n--- Ascent-OBS Executable ---"
Copy-BuildFile (Join-Path $AscentObsDir "x64\Release\ascent-obs.exe") "bin\64bit"

# =======================================================
# 2. Third-party dependencies from .deps
# =======================================================
Write-Host "`n--- Third-party Dependencies (.deps) ---"
$DepsBin = Join-Path $DepsDir "bin"

$DepsDlls = @(
    "avcodec-60.dll"
    "avdevice-60.dll"
    "avfilter-9.dll"
    "avformat-60.dll"
    "avutil-58.dll"
    "swresample-4.dll"
    "swscale-7.dll"
    "libcurl.dll"
    "librist.dll"
    "libx264-164.dll"
    "srt.dll"
    "zlib.dll"
)

foreach ($dll in $DepsDlls) {
    Copy-BuildFile (Join-Path $DepsBin $dll) "bin\64bit"
}

$DepsExes = @("ffmpeg.exe", "ffprobe.exe")
foreach ($exe in $DepsExes) {
    Copy-BuildFile (Join-Path $DepsBin $exe) "bin\64bit"
}

# =======================================================
# 3. C++ Redistributable DLLs
# =======================================================
Write-Host "`n--- C++ Redistributable DLLs ---"

$VcCrtPath = $null
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $VsWhere) {
    $VsPath = & $VsWhere -latest -property installationPath 2>$null
    if ($VsPath) {
        $RedistRoot = Join-Path $VsPath "VC\Redist\MSVC"
        $VersionDirs = Get-ChildItem $RedistRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d' } |
            Sort-Object Name -Descending
        foreach ($vdir in $VersionDirs) {
            $candidate = Join-Path $vdir.FullName "x64"
            $crtDir = Get-ChildItem $candidate -Directory -Filter "Microsoft.VC14*.CRT" -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($crtDir) {
                $VcCrtPath = $crtDir.FullName
                break
            }
        }
    }
}

if ($VcCrtPath) {
    Write-Host "  Found VC++ CRT: $VcCrtPath"
    $CppRedistDlls = @(
        "concrt140.dll"
        "msvcp140.dll"
        "msvcp140_1.dll"
        "msvcp140_2.dll"
        "vcruntime140.dll"
        "vcruntime140_1.dll"
    )
    foreach ($dll in $CppRedistDlls) {
        Copy-BuildFile (Join-Path $VcCrtPath $dll) "bin\64bit"
    }
} else {
    Write-Warning "  Could not locate Visual C++ redistributable DLLs."
    Write-Warning "  Install Visual Studio or set the path manually."
}

# =======================================================
# 4. Core OBS libraries from build -> bin\64bit
# =======================================================
Write-Host "`n--- Core OBS Libraries (bin\64bit) ---"

$CoreLibs = @{
    "deps\w32-pthreads\$Configuration\w32-pthreads.dll" = "bin\64bit"
    "libobs\$Configuration\obs.dll" = "bin\64bit"
    "libobs-d3d11\$Configuration\libobs-d3d11.dll" = "bin\64bit"
    "libobs-opengl\$Configuration\libobs-opengl.dll" = "bin\64bit"
    "libobs-winrt\$Configuration\libobs-winrt.dll" = "bin\64bit"
    "deps\obs-scripting\$Configuration\obs-scripting.dll" = "bin\64bit"
}

foreach ($entry in $CoreLibs.GetEnumerator()) {
    Copy-BuildFile (Join-Path $BuildDir $entry.Key) $entry.Value
}

# =======================================================
# 5. Test executables -> bin\64bit
# =======================================================
Write-Host "`n--- Test Executables (bin\64bit) ---"

$TestExes = @{
    "plugins\obs-ffmpeg\obs-amf-test\$Configuration\obs-amf-test.exe" = "bin\64bit"
    "plugins\obs-ffmpeg\obs-nvenc-test\$Configuration\obs-nvenc-test.exe" = "bin\64bit"
    "plugins\obs-qsv11\obs-qsv-test\$Configuration\obs-qsv-test.exe" = "bin\64bit"
}

foreach ($entry in $TestExes.GetEnumerator()) {
    Copy-BuildFile (Join-Path $BuildDir $entry.Key) $entry.Value
}

# obs-ffmpeg-mux.exe -> renamed to ascentobs-ffmpeg-mux.exe
$MuxSource = Join-Path $RunDir "bin\64bit\obs-ffmpeg-mux.exe"
Copy-BuildFile $MuxSource "bin\64bit" "ascentobs-ffmpeg-mux.exe"

# =======================================================
# 6. Plugin DLLs -> obs-plugins\64bit
# =======================================================
Write-Host "`n--- Plugin DLLs (obs-plugins\64bit) ---"

$Plugins = @(
    "ascent-input-overlay"
    "coreaudio-encoder"
    "image-source"
    "obs-ffmpeg"
    "obs-filters"
    "obs-outputs"
    "obs-qsv11"
    "obs-transitions"
    "obs-x264"
    "rtmp-services"
    "win-capture"
    "win-dshow"
    "win-wasapi"
)

foreach ($plugin in $Plugins) {
    Copy-BuildFile (Join-Path $BuildDir "plugins\$plugin\$Configuration\$plugin.dll") "obs-plugins\64bit"
}

# =======================================================
# 7. Data files: libobs effects
# =======================================================
Write-Host "`n--- Data: libobs effects ---"

$LibobsEffects = @(
    "area.effect", "bicubic_scale.effect", "bilinear_lowres_scale.effect",
    "color.effect", "default.effect", "default_rect.effect",
    "deinterlace_base.effect", "deinterlace_blend.effect", "deinterlace_blend_2x.effect",
    "deinterlace_discard.effect", "deinterlace_discard_2x.effect",
    "deinterlace_linear.effect", "deinterlace_linear_2x.effect",
    "deinterlace_yadif.effect", "deinterlace_yadif_2x.effect",
    "format_conversion.effect", "lanczos_scale.effect", "opaque.effect",
    "premultiplied_alpha.effect", "repeat.effect", "solid.effect"
)

$LibobsRunDir = Join-Path $RunDir "data\libobs"
$LibobsSrcDir = Join-Path $ObsStudioDir "libobs\data"

foreach ($file in $LibobsEffects) {
    $runPath = Join-Path $LibobsRunDir $file
    $srcPath = Join-Path $LibobsSrcDir $file
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\libobs"
    } else {
        Copy-BuildFile $srcPath "data\libobs"
    }
}

# =======================================================
# 8. Data files: obs-filters effects + LUTs
# =======================================================
Write-Host "`n--- Data: obs-filters ---"

$FilterEffects = @(
    "blend_add_filter.effect", "blend_mul_filter.effect", "blend_sub_filter.effect",
    "chroma_key_filter.effect", "chroma_key_filter_v2.effect",
    "color.effect", "color_correction_filter.effect", "color_grade_filter.effect",
    "color_key_filter.effect", "color_key_filter_v2.effect",
    "crop_filter.effect", "hdr_tonemap_filter.effect",
    "luma_key_filter.effect", "luma_key_filter_v2.effect",
    "mask_alpha_filter.effect", "mask_color_filter.effect",
    "rtx_greenscreen.effect", "sharpness.effect"
)

$FilterRunDir = Join-Path $RunDir "data\obs-plugins\obs-filters"
$FilterSrcDir = Join-Path $ObsStudioDir "plugins\obs-filters\data"

foreach ($file in $FilterEffects) {
    $runPath = Join-Path $FilterRunDir $file
    $srcPath = Join-Path $FilterSrcDir $file
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\obs-plugins\obs-filters"
    } else {
        Copy-BuildFile $srcPath "data\obs-plugins\obs-filters"
    }
}

$LutFiles = @("grayscale.cube", "original.cube")
foreach ($file in $LutFiles) {
    $runPath = Join-Path $FilterRunDir "LUTs\$file"
    $srcPath = Join-Path $FilterSrcDir "LUTs\$file"
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\obs-plugins\obs-filters\LUTs"
    } else {
        Copy-BuildFile $srcPath "data\obs-plugins\obs-filters\LUTs"
    }
}

# =======================================================
# 9. Data files: obs-transitions effects
# =======================================================
Write-Host "`n--- Data: obs-transitions ---"

$TransEffects = @(
    "fade_to_color_transition.effect", "fade_transition.effect",
    "luma_wipe_transition.effect", "slide_transition.effect",
    "stinger_matte_transition.effect", "swipe_transition.effect"
)

$TransRunDir = Join-Path $RunDir "data\obs-plugins\obs-transitions"
$TransSrcDir = Join-Path $ObsStudioDir "plugins\obs-transitions\data"

foreach ($file in $TransEffects) {
    $runPath = Join-Path $TransRunDir $file
    $srcPath = Join-Path $TransSrcDir $file
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\obs-plugins\obs-transitions"
    } else {
        Copy-BuildFile $srcPath "data\obs-plugins\obs-transitions"
    }
}

# =======================================================
# 10. Data files: rtmp-services
# =======================================================
Write-Host "`n--- Data: rtmp-services ---"

$RtmpFiles = @("package.json", "services.json")
$RtmpRunDir = Join-Path $RunDir "data\obs-plugins\rtmp-services"
$RtmpSrcDir = Join-Path $ObsStudioDir "plugins\rtmp-services\data"

foreach ($file in $RtmpFiles) {
    $runPath = Join-Path $RtmpRunDir $file
    $srcPath = Join-Path $RtmpSrcDir $file
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\obs-plugins\rtmp-services"
    } else {
        Copy-BuildFile $srcPath "data\obs-plugins\rtmp-services"
    }
}

# =======================================================
# 11. Data files: win-capture (hooks, helpers, config)
# =======================================================
Write-Host "`n--- Data: win-capture ---"

$WinCaptureFiles = @(
    "compatibility.json"
    "package.json"
    "get-graphics-offsets32.exe"
    "get-graphics-offsets64.exe"
    "inject-helper32.exe"
    "inject-helper64.exe"
    "ascent-graphics-hook32.dll"
    "ascent-graphics-hook64.dll"
)

$WinCapRunDir = Join-Path $RunDir "data\obs-plugins\win-capture"
$WinCapSrcDir = Join-Path $ObsStudioDir "plugins\win-capture\data"

foreach ($file in $WinCaptureFiles) {
    $runPath = Join-Path $WinCapRunDir $file
    if (Test-Path $runPath) {
        Copy-BuildFile $runPath "data\obs-plugins\win-capture"
    } elseif ($file -match '\.(json)$') {
        $srcPath = Join-Path $WinCapSrcDir $file
        Copy-BuildFile $srcPath "data\obs-plugins\win-capture"
    } else {
        Write-Warning "  SKIP: $file (not found in rundir)"
        $script:SkipCount++
    }
}

# =======================================================
# Summary
# =======================================================
Write-Host "`n======================================================="
Write-Host "  DONE"
Write-Host "======================================================="
Write-Host "  Copied:  $SuccessCount files" -ForegroundColor Green
if ($SkipCount -gt 0) {
    Write-Host "  Skipped: $SkipCount files (source not found)" -ForegroundColor Yellow
}
if ($ErrorCount -gt 0) {
    Write-Host "  Failed:  $ErrorCount files" -ForegroundColor Red
}
Write-Host "  Target:  $TargetPath"
Write-Host "======================================================="
