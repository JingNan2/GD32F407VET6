# Upload via *.pkt (each file = "/WriteFile name\r\n" + BIN)
# Usage: .\write_fonts_uart.ps1 -Port COM5
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$files = @("font_body.pkt", "font_h5.pkt", "font_h3.pkt", "font_h1.pkt")

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, One
$sp.ReadTimeout = 8000
$sp.WriteTimeout = 60000
$sp.NewLine = "`r`n"
$sp.Open()
Start-Sleep -Milliseconds 300
try {
    foreach ($name in $files) {
        $path = Join-Path $dir $name
        if (-not (Test-Path $path)) { throw "Missing $path - run gen_fonts.ps1" }
        $bytes = [System.IO.File]::ReadAllBytes($path)
        Write-Host "==== send $name ($($bytes.Length) bytes) ===="
        $sp.DiscardInBuffer()

        $off = 0
        while ($off -lt $bytes.Length) {
            $n = [Math]::Min(256, $bytes.Length - $off)
            $sp.Write($bytes, $off, $n)
            $off += $n
            Start-Sleep -Milliseconds 2
        }

        $deadline = [datetime]::UtcNow.AddSeconds(25)
        $buf = ""
        while ([datetime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 80
            if ($sp.BytesToRead -gt 0) {
                $buf += $sp.ReadExisting()
                if ($buf -match "WriteFile OK") { break }
                if ($buf -match "FAIL|ExitWriteFile") { throw "Failed: $buf" }
            }
        }
        Write-Host $buf
        if ($buf -notmatch "WriteFile OK") { throw "No OK for $name" }
        Start-Sleep -Milliseconds 500
    }

    $sp.WriteLine("/ListFiles")
    Start-Sleep -Milliseconds 400
    Write-Host $sp.ReadExisting()
    Write-Host "Done. Reset board."
}
finally {
    $sp.Close()
}
