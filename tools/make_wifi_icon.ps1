param([string]$Out = "$PSScriptRoot\..\src\ui\app.ico", [string]$Preview = "$PSScriptRoot\icon_preview.png")
Add-Type -AssemblyName System.Drawing

$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $color = [System.Drawing.Color]::FromArgb(255, 25, 25, 25)
    $brush = New-Object System.Drawing.SolidBrush($color)
    $stroke = [Math]::Max(1.6, 0.095 * $s)
    $pen = New-Object System.Drawing.Pen($color, $stroke)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round

    $cx = 0.5 * $s
    $cy = 0.66 * $s
    $dotR = 0.095 * $s
    $g.FillEllipse($brush, [single]($cx - $dotR), [single]($cy - $dotR), [single](2 * $dotR), [single](2 * $dotR))

    foreach ($r in @((0.215 * $s), (0.40 * $s))) {
        $m = $r + $stroke / 2
        $rect = New-Object System.Drawing.RectangleF([single]($cx - $m), [single]($cy - $m), [single](2 * $m), [single](2 * $m))
        $g.DrawArc($pen, $rect, 218, 104)
    }

    $g.Dispose()
    $pen.Dispose()
    $brush.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,$ms.ToArray()
    $ms.Dispose()

    if ($s -eq 256 -and $Preview) {
        $bg = New-Object System.Drawing.Bitmap(256, 256)
        $gb = [System.Drawing.Graphics]::FromImage($bg)
        $gb.Clear([System.Drawing.Color]::FromArgb(255, 243, 243, 243))
        $gb.DrawImage($bmp, 0, 0, 256, 256)
        $gb.Dispose()
        $bg.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png)
        $bg.Dispose()
    }
    $bmp.Dispose()
}

$fs = [System.IO.File]::Create($Out)
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $bw.Write([byte]$(if ($s -ge 256) { 0 } else { $s }))
    $bw.Write([byte]$(if ($s -ge 256) { 0 } else { $s }))
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$pngs[$i].Length)
    $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Flush()
$bw.Dispose()
$fs.Dispose()

Write-Output ("ICO OK: " + $Out + " " + (Get-Item $Out).Length + " bytes")

