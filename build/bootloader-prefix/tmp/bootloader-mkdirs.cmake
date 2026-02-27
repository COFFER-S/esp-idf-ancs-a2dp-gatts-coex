# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/root/esp/esp-idf/components/bootloader/subproject"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/tmp"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/src/bootloader-stamp"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/src"
  "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/root/openclaw/workspace/esp-idf-ancs-a2dp-gatts-coex/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
