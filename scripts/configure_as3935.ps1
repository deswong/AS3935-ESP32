#!/usr/bin/env pwsh
<#
.SYNOPSIS
Configure AS3935 Lightning Detector via REST API

.DESCRIPTION
Configures the AS3935 sensor's SPI pins through the device's REST API

.PARAMETER Url
Base URL of the device (e.g., http://192.168.4.1)

.PARAMETER SpiHost
SPI host number (default: 1)

.PARAMETER Sclk
SCLK pin number (default: 14)

.PARAMETER Mosi
MOSI pin number (default: 13)

.PARAMETER Miso
MISO pin number (default: 12)

.PARAMETER Cs
CS pin number (default: 15)

.PARAMETER Irq
IRQ pin number (default: 10)

.PARAMETER StatusOnly
Only fetch current status, don't configure

.EXAMPLE
.\configure_as3935.ps1 -Url http://192.168.4.1

.EXAMPLE
.\configure_as3935.ps1 -Url http://192.168.1.153 -SpiHost 1 -Sclk 14 -Mosi 13 -Miso 12 -Cs 15 -Irq 10
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Url,
    
    [int]$SpiHost = 1,
    [int]$Sclk = 14,
    [int]$Mosi = 13,
    [int]$Miso = 12,
    [int]$Cs = 15,
    [int]$Irq = 10,
    
    [switch]$StatusOnly
)

$baseUrl = $Url.TrimEnd('/')

function Get-AS3935PinsStatus {
    param([string]$BaseUrl)
    
    $endpoint = "$BaseUrl/api/as3935/pins/status"
    Write-Host "`nFetching AS3935 pins status from $endpoint"
    
    try {
        $response = Invoke-WebRequest -Uri $endpoint -TimeoutSec 5
        if ($response.StatusCode -eq 200) {
            $data = $response.Content | ConvertFrom-Json
            Write-Host "✓ Current AS3935 pins:"
            $data | ConvertTo-Json | Write-Host
            return $true
        }
    }
    catch {
        Write-Host "✗ Connection error: $_"
        return $false
    }
}

function Get-AS3935Status {
    param([string]$BaseUrl)
    
    $endpoint = "$BaseUrl/api/as3935/status"
    Write-Host "`nFetching AS3935 status from $endpoint"
    
    try {
        $response = Invoke-WebRequest -Uri $endpoint -TimeoutSec 5
        if ($response.StatusCode -eq 200) {
            $data = $response.Content | ConvertFrom-Json
            Write-Host "✓ Current AS3935 status:"
            $data | ConvertTo-Json | Write-Host
            return $true
        }
    }
    catch {
        Write-Host "✗ Connection error: $_"
        return $false
    }
}

function Set-AS3935Pins {
    param(
        [string]$BaseUrl,
        [int]$SpiHost,
        [int]$Sclk,
        [int]$Mosi,
        [int]$Miso,
        [int]$Cs,
        [int]$Irq
    )
    
    $endpoint = "$BaseUrl/api/as3935/pins/save"
    $payload = @{
        spi_host = $SpiHost
        sclk = $Sclk
        mosi = $Mosi
        miso = $Miso
        cs = $Cs
        irq = $Irq
    } | ConvertTo-Json
    
    Write-Host "Configuring AS3935 pins..."
    Write-Host "  SPI Host: $SpiHost"
    Write-Host "  SCLK Pin: $Sclk"
    Write-Host "  MOSI Pin: $Mosi"
    Write-Host "  MISO Pin: $Miso"
    Write-Host "  CS Pin: $Cs"
    Write-Host "  IRQ Pin: $Irq"
    Write-Host "`nSending POST to $endpoint"
    
    try {
        $response = Invoke-WebRequest -Uri $endpoint `
            -Method Post `
            -Headers @{"Content-Type"="application/json"} `
            -Body $payload `
            -TimeoutSec 5
            
        if ($response.StatusCode -eq 200) {
            Write-Host "✓ Successfully configured AS3935 pins"
            Write-Host "Response: $($response.Content)"
            return $true
        }
        else {
            Write-Host "✗ Failed to configure pins (HTTP $($response.StatusCode))"
            Write-Host "Response: $($response.Content)"
            return $false
        }
    }
    catch {
        Write-Host "✗ Connection error: $_"
        return $false
    }
}

# Main
if ($StatusOnly) {
    Get-AS3935PinsStatus -BaseUrl $baseUrl
    Get-AS3935Status -BaseUrl $baseUrl
    exit 0
}

$success = Set-AS3935Pins -BaseUrl $baseUrl -SpiHost $SpiHost -Sclk $Sclk -Mosi $Mosi -Miso $Miso -Cs $Cs -Irq $Irq

if ($success) {
    Get-AS3935PinsStatus -BaseUrl $baseUrl
    Get-AS3935Status -BaseUrl $baseUrl
    exit 0
}
else {
    exit 1
}
