<#
.SYNOPSIS
    Extracts all Sega AFS archives and inner ZIP/DDS/XMX files from HOTD3 game data.
.DESCRIPTION
    Scans assets/game_data/fs/*.AFS, extracts raw entries, and unpacks nested ZIP archives.
#>

[CmdletBinding()]
param(
    [string]$GameDataPath = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
if (-not $GameDataPath) {
    $GameDataPath = Join-Path $scriptDir "..\assets\game_data"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $scriptDir "..\assets\extracted"
}

$GameDataPath = [System.IO.Path]::GetFullPath($GameDataPath)
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$FsPath = Join-Path $GameDataPath "fs"
if (-not (Test-Path $FsPath)) {
    Write-Error "Could not find fs directory at $FsPath"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null

$SevenZip = "C:\Program Files\7-Zip\7z.exe"
if (-not (Test-Path $SevenZip)) {
    $cmd = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($cmd) { $SevenZip = $cmd.Source }
}

Write-Host "=== HOTD3 Asset Extractor ===" -ForegroundColor Cyan
Write-Host "Source: $FsPath"
Write-Host "Target: $OutputPath`n"

$afsFiles = Get-ChildItem -Path $FsPath -Filter "*.AFS"

foreach ($file in $afsFiles) {
    Write-Host "Processing $($file.Name)..." -ForegroundColor Yellow
    $destDir = Join-Path $OutputPath $file.BaseName
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null

    $fs = [System.IO.File]::OpenRead($file.FullName)
    $br = New-Object System.IO.BinaryReader($fs)

    $magic = [System.Text.Encoding]::ASCII.GetString($br.ReadBytes(4))
    if ($magic -ne "AFS`0") {
        Write-Warning "Skipping $($file.Name): Invalid magic '$magic'"
        $br.Close()
        $fs.Close()
        continue
    }

    $count = $br.ReadUInt32()
    $records = @()
    for ($i = 0; $i -lt $count; $i++) {
        $off = $br.ReadUInt32()
        $len = $br.ReadUInt32()
        $records += [PSCustomObject]@{ Id = $i; Offset = $off; Length = $len; Name = "entry_$i" }
    }

    # Filename directory
    if ($count -gt 0 -and $records[0].Offset -ge (8 + $count * 8 + 8)) {
        $fs.Seek(8 + $count * 8, [System.IO.SeekOrigin]::Begin) | Out-Null
        $dirOffset = $br.ReadUInt32()
        $dirLen = $br.ReadUInt32()

        if ($dirOffset -gt 0 -and $dirOffset -lt $fs.Length) {
            $fs.Seek($dirOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
            for ($i = 0; $i -lt $count; $i++) {
                $nameBytes = $br.ReadBytes(32)
                $nameLen = 0
                while ($nameLen -lt 32 -and $nameBytes[$nameLen] -ne 0) { $nameLen++ }
                if ($nameLen -gt 0) {
                    $records[$i].Name = [System.Text.Encoding]::ASCII.GetString($nameBytes, 0, $nameLen)
                }
                $null = $br.ReadBytes(16)
            }
        }
    }

    # Extract entries
    $extractedCount = 0
    foreach ($rec in $records) {
        if ($rec.Length -eq 0) { continue }
        $fs.Seek($rec.Offset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $bytes = $br.ReadBytes($rec.Length)

        $ext = ""
        if ($bytes.Length -ge 4) {
            if ($bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B) {
                $ext = ".zip"
            } elseif ($bytes[0] -eq 0x80 -and $bytes[1] -eq 0x00) {
                $ext = ".adx"
            } elseif ($bytes[0] -eq 0x4C -and $bytes[1] -eq 0x44 -and $bytes[2] -eq 0x4D -and $bytes[3] -eq 0x58) {
                $ext = ".xmx"
            }
        }

        $rawName = $rec.Name
        $cleanFileName = [System.IO.Path]::GetFileName($rawName)
        if (-not $cleanFileName) {
            $cleanFileName = "entry_$($rec.Id)"
        }

        if ($ext -eq ".adx") {
            if ($cleanFileName.EndsWith(".ad")) {
                $cleanFileName += "x"
            } elseif (-not $cleanFileName.EndsWith(".adx")) {
                $cleanFileName = [System.IO.Path]::ChangeExtension($cleanFileName, ".adx")
            }
        } elseif ($ext -eq ".zip") {
            if (-not $cleanFileName.EndsWith(".zip")) {
                $cleanFileName = [System.IO.Path]::ChangeExtension($cleanFileName, ".zip")
            }
        } elseif ($ext -eq ".xmx") {
            if (-not $cleanFileName.EndsWith(".xmx")) {
                $cleanFileName = [System.IO.Path]::ChangeExtension($cleanFileName, ".xmx")
            }
        } elseif (-not [System.IO.Path]::HasExtension($cleanFileName) -and $ext) {
            $cleanFileName += $ext
        }

        $outFilePath = Join-Path $destDir $cleanFileName
        [System.IO.File]::WriteAllBytes($outFilePath, $bytes)
        $extractedCount++

        # If it's a zip and 7-zip is available, optionally unpack into subdirectory
        if ($ext -eq ".zip" -and $SevenZip) {
            $unpackedDir = Join-Path $destDir ([System.IO.Path]::GetFileNameWithoutExtension($cleanFileName))
            & $SevenZip x -bso0 -bsp0 -y $outFilePath -o"$unpackedDir" 2>$null
        }
    }

    $br.Close()
    $fs.Close()

    Write-Host "  -> Extracted $extractedCount files to $destDir" -ForegroundColor Green
}

Write-Host "`nAll assets extracted successfully to $OutputPath!" -ForegroundColor Cyan
