cmake_minimum_required(VERSION 3.5)
project(LightingControl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)

# OpenGL / GLES / Epoxy
pkg_check_modules(EPOXY REQUIRED epoxy)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESV2 REQUIRED glesv2)

# Audio dependencies
pkg_check_modules(PULSE REQUIRED libpulse)
pkg_check_modules(PULSESIMPLE REQUIRED libpulse-simple)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# GPIO
pkg_check_modules(GPIOD REQUIRED libgpiod)

# I2C helper library (for <i2c/smbus.h> support)
find_library(I2C_LIB NAMES i2c REQUIRED)

# Math library (powf, etc.)
find_library(M_LIB NAMES m REQUIRED)

# Sources
set(SOURCES
    # --------------------
    # HEADERS (.h)
    # --------------------
    engine/engine.h
    engine/renderer.h

    gui/calandar.h
    gui/clock.h
    gui/colorwheelpicker.h
    gui/deltaall.h
    gui/deltagroup.h
    gui/editthemepage.h
    gui/editthemes.h
    gui/editteam.h
    gui/home.h
    gui/imgbutton.h
    gui/keypad.h
    gui/patterns.h
    gui/settings.h
    gui/teamlist.h
    gui/themes.h
    gui/toastmessage.h

    mainwindow.h

    threads/clockthread.h
    threads/doorbellthread.h
    threads/gamethread.h
    threads/mobilelightspoller.h

    tools/bme280.h
    tools/buttonimagemaker.h
    tools/gpiohelper.h
    tools/httphelper.h
    tools/logger.h
    tools/parserhelper.h
    tools/powerswitch.h
    tools/settingsrw.h

    # --------------------
    # SOURCES (.cpp / .c)
    # --------------------
    engine/engine.cpp
    engine/renderer.cpp

    gui/calandar.cpp
    gui/clock.cpp
    gui/colorwheelpicker.cpp
    gui/deltaall.cpp
    gui/deltagroup.cpp
    gui/editthemepage.cpp
    gui/editthemes.cpp
    gui/editteam.cpp
    gui/home.cpp
    gui/patterns.cpp
    gui/settings.cpp
    gui/teamlist.cpp
    gui/themes.cpp
    gui/toastmessage.cpp

    main.cpp
    mainwindow.cpp

    threads/clockthread.cpp
    threads/doorbellthread.cpp
    threads/gamethread.cpp
    threads/mobilelightspoller.cpp

    tools/bme280.c
    tools/gpiohelper.cpp
    tools/httphelper.cpp
    tools/parserhelper.cpp
    tools/powerswitch.cpp
    tools/settingsrw.cpp
)

add_executable(LightingControl ${SOURCES})

# Includes
target_include_directories(LightingControl PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${RSVG_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLESV2_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
    ${PULSESIMPLE_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

# Link
target_link_libraries(LightingControl PRIVATE
    ${GTKMM_LIBRARIES}
    ${RSVG_LIBRARIES}
    ${EPOXY_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GLESV2_LIBRARIES}
    ${PULSE_LIBRARIES}
    ${PULSESIMPLE_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    ${GPIOD_LIBRARIES}
    Threads::Threads
    fftw3f
    CURL::libcurl
    ${I2C_LIB}
    ${M_LIB}
    SQLite::SQLite3
)

# Optional: add pkg-config cflags/ldflags if needed
target_compile_options(LightingControl PRIVATE
    ${GTKMM_CFLAGS_OTHER}
    ${RSVG_CFLAGS_OTHER}
    ${EPOXY_CFLAGS_OTHER}
    ${EGL_CFLAGS_OTHER}
    ${GLESV2_CFLAGS_OTHER}
    ${PULSE_CFLAGS_OTHER}
    ${PULSESIMPLE_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
    ${GPIOD_CFLAGS_OTHER}
)

target_link_options(LightingControl PRIVATE
    ${GTKMM_LDFLAGS_OTHER}
    ${RSVG_LDFLAGS_OTHER}
    ${EPOXY_LDFLAGS_OTHER}
    ${EGL_LDFLAGS_OTHER}
    ${GLESV2_LDFLAGS_OTHER}
    ${PULSE_LDFLAGS_OTHER}
    ${PULSESIMPLE_LDFLAGS_OTHER}
    ${SNDFILE_LDFLAGS_OTHER}
    ${GPIOD_LDFLAGS_OTHER}
)
