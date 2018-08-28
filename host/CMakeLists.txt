cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project (zynthian-stm32controller-host)
include_directories("hidapi/hidapi/")

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework IOKit")
    set(HIDAPI_SOURCE hidapi/mac/hid.c)
endif()

if(UNIX AND NOT APPLE)
    set(HIDAPI_SOURCE hidapi/linux/hid.c)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ludev -lasound")
endif()

add_executable(zynthian-stm32controller-host main.cpp ${HIDAPI_SOURCE})
