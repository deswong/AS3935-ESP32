@echo off
REM Configure AS3935 Lightning Detector via REST API (Windows batch)
REM 
REM Usage:
REM   configure_as3935.bat http://192.168.4.1
REM   configure_as3935.bat http://192.168.4.1 1 14 13 12 15 10

if "%1"=="" (
    echo Usage: configure_as3935.bat URL [spi_host] [sclk] [mosi] [miso] [cs] [irq]
    echo Example: configure_as3935.bat http://192.168.4.1
    echo Example: configure_as3935.bat http://192.168.4.1 1 14 13 12 15 10
    exit /b 1
)

setlocal enabledelayedexpansion

set URL=%1
set SPI_HOST=%2
set SCLK=%3
set MOSI=%4
set MISO=%5
set CS=%6
set IRQ=%7

REM Set defaults
if "!SPI_HOST!"=="" set SPI_HOST=1
if "!SCLK!"=="" set SCLK=14
if "!MOSI!"=="" set MOSI=13
if "!MISO!"=="" set MISO=12
if "!CS!"=="" set CS=15
if "!IRQ!"=="" set IRQ=10

echo Configuring AS3935 pins:
echo   URL: !URL!
echo   SPI Host: !SPI_HOST!
echo   SCLK: !SCLK!
echo   MOSI: !MOSI!
echo   MISO: !MISO!
echo   CS: !CS!
echo   IRQ: !IRQ!
echo.

REM Send configuration
curl -X POST !URL!/api/as3935/pins/save ^
  -H "Content-Type: application/json" ^
  -d "{\"spi_host\": !SPI_HOST!, \"sclk\": !SCLK!, \"mosi\": !MOSI!, \"miso\": !MISO!, \"cs\": !CS!, \"irq\": !IRQ!}"

echo.
echo Getting updated status...
curl -s !URL!/api/as3935/pins/status | findstr .
