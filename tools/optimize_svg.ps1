# optimize_svg.ps1
# Reduces floating point precision in SVG files from 16 decimals to 1
# This dramatically reduces file size and parse time on the Raspberry Pi

$svgDir = Join-Path (Split-Path -Parent $PSScriptRoot) "svg"
$files = @("ishihara_shapes.svg", "ishihara_numbers.svg")

foreach ($file in $files) {
    $path = Join-Path $svgDir $file
    if (-not (Test-Path $path)) {
        Write-Host "Not found: $path" -ForegroundColor Red
        continue
    }

    $original = Get-Item $path
    $content = [System.IO.File]::ReadAllText($path)

    # Round all floating point numbers to 1 decimal place
    $optimized = [System.Text.RegularExpressions.Regex]::Replace(
        $content,
        '\d+\.\d+',
        { param($m) [math]::Round([double]$m.Value, 1).ToString("0.0", [System.Globalization.CultureInfo]::InvariantCulture) }
    )

    [System.IO.File]::WriteAllText($path, $optimized, [System.Text.Encoding]::UTF8)

    $newSize = (Get-Item $path).Length
    $reduction = [math]::Round((1 - $newSize / $original.Length) * 100, 1)
    Write-Host "$file`: $([math]::Round($original.Length/1KB,1)) KB -> $([math]::Round($newSize/1KB,1)) KB (-$reduction%)" -ForegroundColor Green
}

Write-Host ""
Write-Host "Done. Commit the optimized SVGs and push to GitHub." -ForegroundColor Cyan
