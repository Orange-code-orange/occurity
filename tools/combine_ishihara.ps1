param(
  [Parameter(Mandatory=$true)][string]$Source,
  [Parameter(Mandatory=$true)][string]$Output
)

$svgFiles = Get-ChildItem -Path $Source -Filter "*.svg" | Where-Object { $_.Name -notlike "z_*" } | Sort-Object Name

if ($svgFiles.Count -eq 0) {
  Write-Error "No SVG files found in: $Source"
  exit 1
}

# Extract viewBox from first file
$firstContent = Get-Content $svgFiles[0].FullName -Raw
if ($firstContent -match 'viewBox="([^"]+)"') {
  $viewBox = $Matches[1]
} else {
  $viewBox = "0 0 1454 1454"
}

Write-Host "Combining $($svgFiles.Count) SVGs from $Source"
Write-Host "Using viewBox: $viewBox"

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('<?xml version="1.0" encoding="UTF-8"?>')
$lines.Add("<svg xmlns=""http://www.w3.org/2000/svg"" viewBox=""$viewBox"">")

$layerIds = [System.Collections.Generic.List[string]]::new()

for ($i = 0; $i -lt $svgFiles.Count; $i++) {
  $plateId = "plate_{0:D2}" -f ($i + 1)
  $layerIds.Add($plateId)

  $content = Get-Content $svgFiles[$i].FullName -Raw -Encoding UTF8

  # Strip XML declaration and SVG wrapper tags
  $content = $content -replace '^\s*<\?xml[^?]*\?>\s*', ''
  $content = $content -replace '(?s)^\s*<svg[^>]*>', ''
  $content = $content -replace '</svg>\s*$', ''
  $content = $content.Trim()

  $lines.Add("  <g id=""$plateId"">")
  $lines.Add($content)
  $lines.Add("  </g>")

  Write-Host "  Added plate $($i+1)/$($svgFiles.Count): $($svgFiles[$i].Name) -> $plateId"
}

$lines.Add("</svg>")

$outputDir = Split-Path $Output -Parent
if ($outputDir -and -not (Test-Path $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir | Out-Null
}

[System.IO.File]::WriteAllLines($Output, $lines, [System.Text.UTF8Encoding]::new($false))

$sizeMB = [math]::Round((Get-Item $Output).Length / 1MB, 2)
Write-Host "Output: $Output ($sizeMB MB)"
Write-Host ""
Write-Host "Layer IDs for charts.xml:"
foreach ($id in $layerIds) {
  Write-Host "      <layer id=""$id""/>"
}
