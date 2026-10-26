# preparar_usb.ps1
# Copia los archivos de Ishihara al USB con etiqueta USBPEN
# Ejecutar desde PowerShell: .\preparar_usb.ps1

$projectRoot = Split-Path -Parent $PSScriptRoot

# Buscar la unidad USB con etiqueta USBPEN
$usbDrive = Get-Volume | Where-Object { $_.FileSystemLabel -eq "USBPEN" } | Select-Object -First 1

if (-not $usbDrive) {
    Write-Host "ERROR: No se encontro ninguna unidad con etiqueta 'USBPEN'." -ForegroundColor Red
    Write-Host "Asegurate de haber formateado el USB como FAT32 con etiqueta: USBPEN" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Pasos para formatear:" -ForegroundColor Cyan
    Write-Host "  1. Inserta el USB"
    Write-Host "  2. Click derecho en el USB en el Explorador"
    Write-Host "  3. Formatear > FAT32 > Etiqueta de volumen: USBPEN > Iniciar"
    pause
    exit 1
}

$driveLetter = $usbDrive.DriveLetter + ":\"
Write-Host "USB encontrado en: $driveLetter" -ForegroundColor Green

# Crear estructura de carpetas
$folders = @(
    "$driveLetter\occurity\jobs",
    "$driveLetter\occurity\svg"
)

foreach ($folder in $folders) {
    if (-not (Test-Path $folder)) {
        New-Item -ItemType Directory -Path $folder -Force | Out-Null
        Write-Host "Carpeta creada: $folder"
    }
}

# Archivos a copiar
$files = @(
    @{ Src = "$projectRoot\svg\ishihara_shapes.svg";   Dst = "$driveLetter\occurity\svg\ishihara_shapes.svg" },
    @{ Src = "$projectRoot\svg\ishihara_numbers.svg";  Dst = "$driveLetter\occurity\svg\ishihara_numbers.svg" },
    @{ Src = "$projectRoot\charts.xml.example";        Dst = "$driveLetter\occurity\charts.xml.example" },
    @{ Src = "$projectRoot\tools\deploy_ishihara.job"; Dst = "$driveLetter\occurity\jobs\deploy_ishihara.job" }
)

$errors = 0
foreach ($file in $files) {
    if (Test-Path $file.Src) {
        Copy-Item -Path $file.Src -Destination $file.Dst -Force
        Write-Host "Copiado: $($file.Dst)" -ForegroundColor Green
    } else {
        Write-Host "FALTA: $($file.Src)" -ForegroundColor Red
        $errors++
    }
}

Write-Host ""
if ($errors -eq 0) {
    Write-Host "USB listo. Inserta el USB en la Raspberry Pi y ejecuta el job 'Instalar pruebas Ishihara' desde el Job Runner (tecla j, PIN 4242)." -ForegroundColor Cyan
} else {
    Write-Host "Hubo $errors errores. Verifica los archivos faltantes." -ForegroundColor Red
}
pause
