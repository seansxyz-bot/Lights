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
    # h
    mainwindow.h
    gui/calandar.h
    gui/clock.h
    gui/colorwheelpicker.h
    gui/deltaall.h
    gui/deltagroup.h
    gui/gamedayscreen.h
    gui/home.h
    gui/imgbutton.h
    gui/keypad.h
    gui/patterns.h
    gui/settings.h

    gui/themes.h
    threads/clockthread.h
    threads/gamethread.h
    tools/bme280.h

    tools/logger.h
    tools/settingsrw.h
    tools/sportsschedules.h

    # cpp / c
    main.cpp
    mainwindow.cpp
    gui/calandar.cpp
    gui/clock.cpp
    gui/colorwheelpicker.cpp
    gui/deltaall.cpp
    gui/deltagroup.cpp
    gui/gamedayscreen.cpp
    gui/home.cpp
    gui/patterns.cpp
    gui/settings.cpp

    gui/themes.cpp
    threads/clockthread.cpp
    threads/gamethread.cpp
    tools/bme280.c

    tools/settingsrw.cpp
    tools/sportsschedules.cpp
)

add_executable(LightingControl
    ${SOURCES}
    tools/gpiohelper.h tools/gpiohelper.cpp
    tools/powerswitch.h tools/powerswitch.cpp
    tools/httphelper.h tools/httphelper.cpp
    threads/doorbellthread.h threads/doorbellthread.cpp
    threads/mobilelightspoller.h threads/mobilelightspoller.cpp
    gui/editthemes.h gui/editthemes.cpp
    gui/editthemepage.h gui/editthemepage.cpp
    tools/buttonimagemaker.h
    gui/toastmessage.h gui/toastmessage.cpp
    gui/teamlist.h gui/teamlist.cpp
    gui/editteam.h gui/editteam.cpp

)

# Includes
target_include_directories(LightingControl PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${RSVG_INCLUDE_DIRS}
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
    ${PULSE_CFLAGS_OTHER}
    ${PULSESIMPLE_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
    ${GPIOD_CFLAGS_OTHER}
)

target_link_options(LightingControl PRIVATE
    ${GTKMM_LDFLAGS_OTHER}
    ${RSVG_LDFLAGS_OTHER}
    ${PULSE_LDFLAGS_OTHER}
    ${PULSESIMPLE_LDFLAGS_OTHER}
    ${SNDFILE_LDFLAGS_OTHER}
    ${GPIOD_LDFLAGS_OTHER}
)
