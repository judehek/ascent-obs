param(
    [string]$outputFile = "filenames_list.txt"
)

# Function to recursively process directories
function Process-Directory {
    param (
        [string]$path,
        [System.IO.StreamWriter]$writer,
        [string]$scriptToExclude,
        [string]$outputFileToExclude
    )
    
    # Get all files in the current directory
    $files = Get-ChildItem -Path $path -File

    foreach ($file in $files) {
        # Skip the script file itself and the output file
        if ($file.Name -eq $scriptToExclude -or $file.Name -eq $outputFileToExclude) {
            continue
        }

        $relativePath = $file.FullName.Substring((Get-Location).Path.Length + 1)
        
        # Write just the file path to the output file
        $writer.WriteLine($relativePath)
    }

    # Process subdirectories recursively
    $subdirs = Get-ChildItem -Path $path -Directory
    foreach ($dir in $subdirs) {
        Process-Directory -path $dir.FullName -writer $writer -scriptToExclude $scriptToExclude -outputFileToExclude $outputFileToExclude
    }
}

# Make sure we use absolute path for the output file
$absoluteOutputPath = Join-Path -Path (Get-Location).Path -ChildPath $outputFile

# Get the script filename to exclude it
$scriptName = $MyInvocation.MyCommand.Name

# Create output file
try {
    Write-Host "Creating file at: $absoluteOutputPath"
    
    # Create the StreamWriter with the absolute path
    $writer = New-Object System.IO.StreamWriter $absoluteOutputPath
    
    # Start processing from current directory
    Process-Directory -path (Get-Location).Path -writer $writer -scriptToExclude $scriptName -outputFileToExclude $outputFile
    
    Write-Host "File collection completed. Output saved to: $absoluteOutputPath"
}
catch {
    Write-Error "Error: $_"
}
finally {
    if ($writer) {
        $writer.Close()
    }
    
    # Check if file exists after writing
    if (Test-Path $absoluteOutputPath) {
        $fileInfo = Get-Item $absoluteOutputPath
        Write-Host "Output file: $($fileInfo.FullName)"
        Write-Host "Output file size: $($fileInfo.Length) bytes"
    } else {
        Write-Host "WARNING: File not found at expected location: $absoluteOutputPath"
    }
}