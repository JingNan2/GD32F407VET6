# Generate Assets/cjk14.bin for EFS + LVGL binfont (dynamic glyph load)
param(
    [Parameter(Mandatory = $true)]
    [string]$FontPath,
    [string]$OutPath = "",
    [string]$SymbolsFile = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ([string]::IsNullOrWhiteSpace($OutPath)) {
    $OutPath = Join-Path $root "Assets\cjk14.bin"
}
if ([string]::IsNullOrWhiteSpace($SymbolsFile)) {
    $SymbolsFile = Join-Path $root "Assets\cjk14_symbols.txt"
}

if (-not (Test-Path $FontPath)) {
    throw "Font not found: $FontPath"
}
if (-not (Test-Path $SymbolsFile)) {
    throw "Symbols file not found: $SymbolsFile"
}

$conv = Get-Command lv_font_conv -ErrorAction SilentlyContinue
if ($null -eq $conv) {
    Write-Host "Installing lv_font_conv globally..."
    npm i -g lv_font_conv
}

$outDir = Split-Path -Parent $OutPath
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

# lv_font_conv only has --symbols (string), not --symbols-file
$symbols = [System.IO.File]::ReadAllText($SymbolsFile, [System.Text.UTF8Encoding]::new($false))

Write-Host "Converting -> $OutPath (symbols=$($symbols.Length))"
& lv_font_conv `
    --font $FontPath `
    --size 14 --bpp 1 `
    --no-compress --no-prefilter `
    -r 0x20-0x7F `
    --symbols $symbols `
    --format bin `
    -o $OutPath

if (($LASTEXITCODE -ne 0) -or (-not (Test-Path $OutPath))) {
    throw "lv_font_conv failed"
}

$len = (Get-Item $OutPath).Length
Write-Host "OK size=$len bytes"
Write-Host "Upload: serial /WriteFile cjk14.bin  then send this BIN"
