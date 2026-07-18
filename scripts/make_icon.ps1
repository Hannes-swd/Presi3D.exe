# Builds resources/app.ico (multi-resolution, PNG-compressed frames) from
# resources/branding/app_icon.png. Re-run this whenever the source artwork changes.
Add-Type -AssemblyName System.Drawing

$src = Join-Path $PSScriptRoot "..\resources\branding\app_icon.png"
$dst = Join-Path $PSScriptRoot "..\resources\app.ico"
$sizes = 16, 32, 48, 64, 128, 256

$srcImg = [System.Drawing.Image]::FromFile($src)

$pngBlobs = foreach ($size in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($srcImg, 0, 0, $size, $size)
    $g.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    ,$ms.ToArray()
}
$srcImg.Dispose()

$fs = [System.IO.File]::Create($dst)
$bw = New-Object System.IO.BinaryWriter $fs

# ICONDIR
$bw.Write([UInt16]0)              # reserved
$bw.Write([UInt16]1)              # type = icon
$bw.Write([UInt16]$sizes.Count)   # image count

$headerSize = 6 + 16 * $sizes.Count
$offset = $headerSize

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $size = $sizes[$i]
    $blob = $pngBlobs[$i]
    $dim = if ($size -ge 256) { 0 } else { $size }  # 0 means 256 in ICO format
    $bw.Write([Byte]$dim)          # width
    $bw.Write([Byte]$dim)          # height
    $bw.Write([Byte]0)             # color count
    $bw.Write([Byte]0)             # reserved
    $bw.Write([UInt16]1)           # color planes
    $bw.Write([UInt16]32)          # bits per pixel
    $bw.Write([UInt32]$blob.Length)
    $bw.Write([UInt32]$offset)
    $offset += $blob.Length
}

foreach ($blob in $pngBlobs) {
    $bw.Write($blob)
}

$bw.Flush()
$bw.Close()
$fs.Close()

Write-Host "Wrote $dst ($($sizes -join ', ') px)"
