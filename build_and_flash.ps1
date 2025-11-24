#!/usr/bin/env pwsh
# AS3935-ESP32 Build and Flash Script
# Run this script to build and flash the latest firmware

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          AS3935-ESP32 Build and Flash Script                 ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Setup environment variables
Write-Host "[1/4] Setting up build environment..." -ForegroundColor Yellow
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"

# Change to project directory
Set-Location d:\AS3935-ESP32
Write-Host "✓ Environment ready" -ForegroundColor Green
Write-Host ""

# Clean build
Write-Host "[2/4] Cleaning previous build..." -ForegroundColor Yellow
python -m idf.py fullclean 2>&1 | Out-Null
Write-Host "✓ Build directory cleaned" -ForegroundColor Green
Write-Host ""

# Build
Write-Host "[3/4] Building firmware (this may take 1-2 minutes)..." -ForegroundColor Yellow
$buildOutput = python -m idf.py build 2>&1
$buildOutput | Select-Object -Last 20

# Check if build succeeded
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Build successful!" -ForegroundColor Green
    Write-Host ""
    
    # Ask user if they want to flash
    Write-Host "[4/4] Flash to ESP32?" -ForegroundColor Yellow
    Write-Host "Make sure ESP32 is connected via USB to the correct COM port" -ForegroundColor Cyan
    $response = Read-Host "Enter COM port (e.g., COM3) or press Enter to skip"
    
    if ($response) {
        Write-Host "Flashing to $response..." -ForegroundColor Yellow
        python -m idf.py -p $response flash monitor
        Write-Host "✓ Flash complete!" -ForegroundColor Green
    } else {
        Write-Host "Skipping flash. When ready, run: python -m idf.py -p COMX flash monitor" -ForegroundColor Cyan
    }
} else {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    Write-Host "See errors above. Common issues:" -ForegroundColor Yellow
    Write-Host "  - Compiler not in PATH" -ForegroundColor Cyan
    Write-Host "  - Build directory corrupted (try manual: idf.py fullclean)" -ForegroundColor Cyan
    Write-Host "  - Missing dependencies" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                       Done!                                  ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
