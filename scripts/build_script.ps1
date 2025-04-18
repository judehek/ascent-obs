# --- Configuration ---
$ScriptName = $MyInvocation.MyCommand.Name
$BaseSourceDirectory = $PSScriptRoot # Assumes script is run from the parent of .deps, build_x64, libobs, plugins
$DesktopPath = [Environment]::GetFolderPath("Desktop")
$TargetRootFolderName = "OBS_Organized_Build" # Changed name slightly to avoid conflict if you ran the move script
$TargetRootPath = Join-Path -Path $DesktopPath -ChildPath $TargetRootFolderName

Write-Host "-----------------------------------------------------"
Write-Host " OBS Build File Organizer (Copy Mode)"
Write-Host "-----------------------------------------------------"
Write-Host "Source Base Path: $BaseSourceDirectory"
Write-Host "Target Path:      $TargetRootPath"
Write-Host "Mode:             Copying files (source files will remain)"
Write-Host "IMPORTANT: This script will overwrite existing files in the target!"
Write-Host "-----------------------------------------------------"
# Read-Host -Prompt "Press Enter to continue, or CTRL+C to cancel" # Uncomment for safety pause

# --- File Mappings (Source Relative Path = Target Relative Directory) ---
# This maps the source file path (relative to $BaseSourceDirectory)
# to the destination directory path (relative to $TargetRootPath)
$FileMappings = @{
    # --- .deps x64 Files ---
    '.deps\obs-deps-2024-03-19-x64\bin\avcodec-60.dll'   = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\avdevice-60.dll'  = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\avfilter-9.dll'   = 'bin\64bit' # Target list shows -10, using source
    '.deps\obs-deps-2024-03-19-x64\bin\avformat-60.dll'  = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\avutil-58.dll'    = 'bin\64bit' # Target list shows -59, using source
    '.deps\obs-deps-2024-03-19-x64\bin\ffmpeg.exe'      = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\ffprobe.exe'     = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\libcurl.dll'     = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\librist.dll'     = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\libx264-164.dll'  = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\srt.dll'         = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\swresample-4.dll' = 'bin\64bit' # Target list shows -5, using source
    '.deps\obs-deps-2024-03-19-x64\bin\swscale-7.dll'    = 'bin\64bit' # Target list shows -8, using source
    '.deps\obs-deps-2024-03-19-x64\bin\zlib.dll'        = 'bin\64bit'
    '.deps\obs-deps-2024-03-19-x64\bin\postproc-58.dll' = 'bin\64bit' # Assuming postproc exists in deps

    # --- build_x64 Files ---
    # Core libs / Binaries -> bin\64bit
    'build_x64\deps\w32-pthreads\Release\w32-pthreads.dll' = 'bin\64bit'
    'build_x64\libobs\Release\obs.dll'                     = 'bin\64bit'
    'build_x64\libobs-d3d11\Release\libobs-d3d11.dll'       = 'bin\64bit'
    'build_x64\libobs-opengl\Release\libobs-opengl.dll'     = 'bin\64bit'
    'build_x64\libobs-winrt\Release\libobs-winrt.dll'      = 'bin\64bit'
    'build_x64\plugins\obs-ffmpeg\obs-amf-test\Release\obs-amf-test.exe' = 'bin\64bit'
    'build_x64\plugins\obs-ffmpeg\obs-nvenc-test\Release\obs-nvenc-test.exe' = 'bin\64bit'
    'build_x64\plugins\obs-qsv11\obs-qsv-test\Release\obs-qsv-test.exe' = 'bin\64bit'
    'build_x64\rundir\Release\bin\64bit\obs-ffmpeg-mux.exe' = 'bin\64bit'

    # Plugin DLLs -> obs-plugins\64bit
    'build_x64\plugins\coreaudio-encoder\Release\coreaudio-encoder.dll' = 'obs-plugins\64bit'
    'build_x64\plugins\image-source\Release\image-source.dll'           = 'obs-plugins\64bit'
    'build_x64\plugins\obs-ffmpeg\Release\obs-ffmpeg.dll'               = 'obs-plugins\64bit'
    'build_x64\plugins\obs-filters\Release\obs-filters.dll'             = 'obs-plugins\64bit'
    'build_x64\plugins\obs-outputs\Release\obs-outputs.dll'             = 'obs-plugins\64bit'
    'build_x64\plugins\obs-qsv11\Release\obs-qsv11.dll'                 = 'obs-plugins\64bit'
    'build_x64\plugins\obs-transitions\Release\obs-transitions.dll'     = 'obs-plugins\64bit'
    'build_x64\plugins\obs-x264\Release\obs-x264.dll'                   = 'obs-plugins\64bit'
    'build_x64\plugins\rtmp-services\Release\rtmp-services.dll'         = 'obs-plugins\64bit'
    'build_x64\plugins\win-capture\Release\win-capture.dll'             = 'obs-plugins\64bit'
    'build_x64\plugins\win-dshow\Release\win-dshow.dll'                 = 'obs-plugins\64bit'
    'build_x64\plugins\win-wasapi\Release\win-wasapi.dll'               = 'obs-plugins\64bit'

    # Data files (Effects, JSON, etc.) -> data\...
    'build_x64\rundir\Release\data\libobs\area.effect'                  = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\bicubic_scale.effect'         = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\bilinear_lowres_scale.effect' = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\color.effect'                 = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\default.effect'               = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\default_rect.effect'          = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_base.effect'      = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_blend.effect'     = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_blend_2x.effect'  = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_discard.effect'   = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_discard_2x.effect'= 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_linear.effect'    = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_linear_2x.effect' = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_yadif.effect'     = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\deinterlace_yadif_2x.effect'  = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\format_conversion.effect'     = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\lanczos_scale.effect'         = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\opaque.effect'                = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\premultiplied_alpha.effect'   = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\repeat.effect'                = 'data\libobs'
    'build_x64\rundir\Release\data\libobs\solid.effect'                 = 'data\libobs'

    'build_x64\rundir\Release\data\obs-plugins\obs-filters\blend_add_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\blend_mul_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\blend_sub_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\chroma_key_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\chroma_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\color.effect'            = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\color_correction_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\color_grade_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\color_key_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\color_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\crop_filter.effect'      = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\hdr_tonemap_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\luma_key_filter.effect'  = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\luma_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\mask_alpha_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\mask_color_filter.effect' = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\rtx_greenscreen.effect'  = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\sharpness.effect'        = 'data\obs-plugins\obs-filters'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\LUTs\grayscale.cube'    = 'data\obs-plugins\obs-filters\LUTs'
    'build_x64\rundir\Release\data\obs-plugins\obs-filters\LUTs\original.cube'     = 'data\obs-plugins\obs-filters\LUTs'

    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\fade_to_color_transition.effect' = 'data\obs-plugins\obs-transitions'
    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\fade_transition.effect'          = 'data\obs-plugins\obs-transitions'
    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\luma_wipe_transition.effect'     = 'data\obs-plugins\obs-transitions'
    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\slide_transition.effect'         = 'data\obs-plugins\obs-transitions'
    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\stinger_matte_transition.effect' = 'data\obs-plugins\obs-transitions'
    'build_x64\rundir\Release\data\obs-plugins\obs-transitions\swipe_transition.effect'         = 'data\obs-plugins\obs-transitions'

    'build_x64\rundir\Release\data\obs-plugins\rtmp-services\package.json' = 'data\obs-plugins\rtmp-services'
    'build_x64\rundir\Release\data\obs-plugins\rtmp-services\services.json'= 'data\obs-plugins\rtmp-services'

    'build_x64\rundir\Release\data\obs-plugins\win-capture\compatibility.json' = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\get-graphics-offsets32.exe' = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\get-graphics-offsets64.exe' = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\inject-helper32.exe'       = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\inject-helper64.exe'       = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\ascent-graphics-hook32.dll'     = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\ascent-graphics-hook64.dll'     = 'data\obs-plugins\win-capture'
    'build_x64\rundir\Release\data\obs-plugins\win-capture\package.json'              = 'data\obs-plugins\win-capture'

    # --- Files from source root structure (libobs/plugins) ---
    'libobs\data\area.effect'                   = 'data\libobs'
    'libobs\data\bicubic_scale.effect'          = 'data\libobs'
    'libobs\data\bilinear_lowres_scale.effect'  = 'data\libobs'
    'libobs\data\color.effect'                  = 'data\libobs'
    'libobs\data\default.effect'                = 'data\libobs'
    'libobs\data\default_rect.effect'           = 'data\libobs'
    'libobs\data\deinterlace_base.effect'       = 'data\libobs'
    'libobs\data\deinterlace_blend.effect'      = 'data\libobs'
    'libobs\data\deinterlace_blend_2x.effect'   = 'data\libobs'
    'libobs\data\deinterlace_discard.effect'    = 'data\libobs'
    'libobs\data\deinterlace_discard_2x.effect' = 'data\libobs'
    'libobs\data\deinterlace_linear.effect'     = 'data\libobs'
    'libobs\data\deinterlace_linear_2x.effect'  = 'data\libobs'
    'libobs\data\deinterlace_yadif.effect'      = 'data\libobs'
    'libobs\data\deinterlace_yadif_2x.effect'   = 'data\libobs'
    'libobs\data\format_conversion.effect'      = 'data\libobs'
    'libobs\data\lanczos_scale.effect'          = 'data\libobs'
    'libobs\data\opaque.effect'                 = 'data\libobs'
    'libobs\data\premultiplied_alpha.effect'    = 'data\libobs'
    'libobs\data\repeat.effect'                 = 'data\libobs'
    'libobs\data\solid.effect'                  = 'data\libobs'

    'plugins\obs-filters\data\blend_add_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\blend_mul_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\blend_sub_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\chroma_key_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\chroma_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\color.effect'            = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\color_correction_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\color_grade_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\color_key_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\color_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\crop_filter.effect'      = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\hdr_tonemap_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\luma_key_filter.effect'  = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\luma_key_filter_v2.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\mask_alpha_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\mask_color_filter.effect' = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\rtx_greenscreen.effect'  = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\sharpness.effect'        = 'data\obs-plugins\obs-filters'
    'plugins\obs-filters\data\LUTs\grayscale.cube'    = 'data\obs-plugins\obs-filters\LUTs'
    'plugins\obs-filters\data\LUTs\original.cube'     = 'data\obs-plugins\obs-filters\LUTs'

    'plugins\obs-transitions\data\fade_to_color_transition.effect' = 'data\obs-plugins\obs-transitions'
    'plugins\obs-transitions\data\fade_transition.effect'          = 'data\obs-plugins\obs-transitions'
    'plugins\obs-transitions\data\luma_wipe_transition.effect'     = 'data\obs-plugins\obs-transitions'
    'plugins\obs-transitions\data\slide_transition.effect'         = 'data\obs-plugins\obs-transitions'
    'plugins\obs-transitions\data\stinger_matte_transition.effect' = 'data\obs-plugins\obs-transitions'
    'plugins\obs-transitions\data\swipe_transition.effect'         = 'data\obs-plugins\obs-transitions'

    'plugins\rtmp-services\data\package.json'         = 'data\obs-plugins\rtmp-services'
    'plugins\rtmp-services\data\services.json'        = 'data\obs-plugins\rtmp-services'
    'plugins\win-capture\data\compatibility.json'     = 'data\obs-plugins\win-capture'
    'plugins\win-capture\data\package.json'           = 'data\obs-plugins\win-capture'

    # --- Add mappings for any missing target files if they exist in your build output ---
    # Example: Assuming obs-nvenc.dll is built here:
    # 'build_x64\rundir\Release\obs-plugins\64bit\obs-nvenc.dll' = 'obs-plugins\64bit'
    # Example: Assuming obs-scripting.dll is built here:
    # 'build_x64\rundir\Release\bin\64bit\obs-scripting.dll' = 'bin\64bit'
}

# --- Create Base Target Directory ---
Write-Host "Creating target directory: $TargetRootPath"
try {
    New-Item -Path $TargetRootPath -ItemType Directory -Force -ErrorAction Stop | Out-Null
} catch {
    Write-Error "Failed to create target directory '$TargetRootPath'. Check permissions. Error: $($_.Exception.Message)"
    Exit 1
}

# --- Process Files ---
Write-Host "Processing file copies..."
$FileMappings.GetEnumerator() | ForEach-Object {
    $SourceRelativePath = $_.Key
    $TargetRelativeDir = $_.Value

    $FullSourcePath = Join-Path -Path $BaseSourceDirectory -ChildPath $SourceRelativePath
    $FullTargetDirPath = Join-Path -Path $TargetRootPath -ChildPath $TargetRelativeDir
    $FileName = Split-Path -Path $FullSourcePath -Leaf

    # Check if source file exists
    if (!(Test-Path -Path $FullSourcePath -PathType Leaf)) {
        Write-Warning "Source file not found: '$FullSourcePath'. Skipping."
        return # Continue to next file
    }

    # Ensure target directory exists
    try {
        if (!(Test-Path -Path $FullTargetDirPath)) {
            Write-Verbose "Creating directory: $FullTargetDirPath"
            New-Item -Path $FullTargetDirPath -ItemType Directory -Force -ErrorAction Stop | Out-Null
        }
    } catch {
        Write-Error "Failed to create target sub-directory '$FullTargetDirPath'. Check permissions. Error: $($_.Exception.Message)"
        return # Skip this file if directory creation fails
    }

    # Construct full target file path
    $FullTargetFilePath = Join-Path -Path $FullTargetDirPath -ChildPath $FileName

    # Copy the file
    Write-Host "Copying '$FileName' from '$SourceRelativePath' to '$TargetRelativeDir'"
    try {
        # --- MODIFIED LINE ---
        Copy-Item -Path $FullSourcePath -Destination $FullTargetFilePath -Force -ErrorAction Stop -Verbose
    } catch {
        # --- MODIFIED ERROR MESSAGE CONTEXT ---
        Write-Error "Failed to copy '$FullSourcePath' to '$FullTargetFilePath'. Error: $($_.Exception.Message)"
        # Optional: Decide whether to stop script or continue with next file
    }
}

# --- Add rename step for obs-ffmpeg-mux.exe to ascentobs-ffmpeg-mux.exe ---
$SourceFile = Join-Path -Path $TargetRootPath -ChildPath "bin\64bit\obs-ffmpeg-mux.exe"
$TargetFile = Join-Path -Path $TargetRootPath -ChildPath "bin\64bit\ascentobs-ffmpeg-mux.exe"

if (Test-Path -Path $SourceFile -PathType Leaf) {
    Write-Host "Renaming 'obs-ffmpeg-mux.exe' to 'ascentobs-ffmpeg-mux.exe'"
    try {
        Copy-Item -Path $SourceFile -Destination $TargetFile -Force -ErrorAction Stop
        Remove-Item -Path $SourceFile -Force -ErrorAction SilentlyContinue
    } catch {
        Write-Error "Failed to rename '$SourceFile' to '$TargetFile'. Error: $($_.Exception.Message)"
    }
} else {
    Write-Warning "Source file for rename not found: '$SourceFile'. Skipping rename operation."
}

Write-Host "-----------------------------------------------------"
Write-Host "File organization (copy) process completed."
Write-Host "Check the '$TargetRootPath' folder."
Write-Host "Original source files remain in place."
Write-Host "-----------------------------------------------------"