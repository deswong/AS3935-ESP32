# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/des/esp/v5.5.1/esp-idf/components/bootloader/subproject"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/tmp"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/src/bootloader-stamp"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/src"
  "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/des/Documents/GitHub/AS3935-ESP32/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
