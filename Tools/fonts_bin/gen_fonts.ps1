# Regenerate LVGL binary fonts + .pkt (command line prepended for one-click UART send)
$ErrorActionPreference = "Stop"
$fontDir = "C:\Users\Administrator\Documents\lvgl_ui_project\fonts"
$outDir = Split-Path -Parent $MyInvocation.MyCommand.Path

npx --yes lv_font_conv --font "$fontDir\Montserrat-Regular.ttf" -r 0x20-0x7F --symbols "°—•" --size 16 --bpp 4 --format bin -o "$outDir\font_body.bin"
npx --yes lv_font_conv --font "$fontDir\Montserrat-Medium.ttf"  -r 0x20-0x7F --symbols "°—•" --size 18 --bpp 4 --format bin -o "$outDir\font_h5.bin"
npx --yes lv_font_conv --font "$fontDir\Montserrat-SemiBold.ttf" -r 0x20-0x7F --symbols "°—•" --size 28 --bpp 4 --format bin -o "$outDir\font_h3.bin"
npx --yes lv_font_conv --font "$fontDir\Montserrat-Bold.ttf"   -r 0x20-0x7F --symbols "°—•" --size 44 --bpp 4 --format bin -o "$outDir\font_h1.bin"

foreach ($name in @("font_body.bin","font_h5.bin","font_h3.bin","font_h1.bin")) {
  $bin = [IO.File]::ReadAllBytes((Join-Path $outDir $name))
  $cmd = [Text.Encoding]::ASCII.GetBytes("/WriteFile $name`r`n")
  $pkt = New-Object byte[] ($cmd.Length + $bin.Length)
  [Array]::Copy($cmd, 0, $pkt, 0, $cmd.Length)
  [Array]::Copy($bin, 0, $pkt, $cmd.Length, $bin.Length)
  $pktName = $name -replace '\.bin$','.pkt'
  [IO.File]::WriteAllBytes((Join-Path $outDir $pktName), $pkt)
}

Get-ChildItem $outDir\*.bin,$outDir\*.pkt | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "Send *.pkt with serial 'send file' (includes /WriteFile command)."
