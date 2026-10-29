cmake_minimum_required(VERSION 3.24)
project(LiveStudio VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---- lokasi hasil build/instalasi libobs (lihat docs/BUILD_WINDOWS.md) ----
set(OBS_INSTALL_DIR "" CACHE PATH "Folder hasil 'cmake --install' OBS Studio (berisi include/ dan lib/ libobs)")
set(LIVESTUDIO_OBS_RUNTIME_DIR "${OBS_INSTALL_DIR}" CACHE PATH
    "Sumber runtime OBS (bin/64bit, obs-plugins, data) yang disalin saat 'cmake --install'")
if(OBS_INSTALL_DIR)
  list(APPEND CMAKE_PREFIX_PATH "${OBS_INSTALL_DIR}")
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# 1) Coba paket CMake resmi libobs (target OBS::libobs).
find_package(libobs CONFIG QUIET)

# 2) Cadangan: cari header & library secara manual.
if(NOT TARGET OBS::libobs)
  find_path(LIBOBS_INCLUDE_DIR NAMES obs.h
            HINTS "${OBS_INSTALL_DIR}" ENV OBS_INSTALL_DIR
            PATH_SUFFIXES include/obs include libobs)
  find_library(LIBOBS_LIBRARY NAMES obs libobs
               HINTS "${OBS_INSTALL_DIR}" ENV OBS_INSTALL_DIR
               PATH_SUFFIXES lib lib64 bin/64bit lib/x64 libobs)
  if(NOT LIBOBS_INCLUDE_DIR OR NOT LIBOBS_LIBRARY)
    message(FATAL_ERROR
      "libobs tidak ditemukan. Isi -DOBS_INSTALL_DIR=<folder hasil cmake --install OBS> "
      "atau set LIBOBS_INCLUDE_DIR dan LIBOBS_LIBRARY. Lihat docs/BUILD_WINDOWS.md.")
  endif()
  # obs-config.h mewajibkan obsconfig.h hasil generate; threading.h memakai <pthread.h> (w32-pthreads di Windows).
  find_path(LIBOBS_CONFIG_DIR NAMES obsconfig.h
            HINTS "${OBS_INSTALL_DIR}" "${LIBOBS_INCLUDE_DIR}" ENV OBS_INSTALL_DIR
            PATH_SUFFIXES include/obs include config .)
  set(_obs_incs "${LIBOBS_INCLUDE_DIR}")
  if(LIBOBS_CONFIG_DIR)
    list(APPEND _obs_incs "${LIBOBS_CONFIG_DIR}")
  endif()
  if(WIN32)
    set(W32_PTHREADS_INCLUDE_DIR "" CACHE PATH "Folder berisi pthread.h (w32-pthreads dari obs-deps)")
    if(W32_PTHREADS_INCLUDE_DIR)
      list(APPEND _obs_incs "${W32_PTHREADS_INCLUDE_DIR}")
    endif()
  endif()
  add_library(OBS::libobs UNKNOWN IMPORTED)
  set_target_properties(OBS::libobs PROPERTIES
    IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${_obs_incs}")
  message(STATUS "libobs (manual): ${LIBOBS_LIBRARY}  headers: ${LIBOBS_INCLUDE_DIR}")
else()
  message(STATUS "libobs (paket CMake): ditemukan")
endif()

set(LS_CORE_SOURCES
  src/core/Bumper.cpp
  src/core/LowerThird.cpp
  src/core/ObsEngine.cpp
  src/core/SceneManager.cpp
  src/core/SelfTest.cpp
  src/core/SourceFactory.cpp
  src/ui/AudioMixerWidget.cpp
  src/ui/BumperPanel.cpp
  src/ui/LowerThirdPanel.cpp
  src/ui/MainWindow.cpp
  src/ui/ObsDisplayWidget.cpp
  src/ui/ProjectorWindow.cpp
  src/ui/Render.cpp
)

add_library(livestudio_core STATIC ${LS_CORE_SOURCES})
target_include_directories(livestudio_core PUBLIC src)
target_link_libraries(livestudio_core PUBLIC Qt6::Widgets OBS::libobs)

if(WIN32)
  target_compile_definitions(livestudio_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(livestudio_core PUBLIC /W3 /utf-8 /permissive- /Zc:__cplusplus)
  endif()
  set(LS_APP_SOURCES src/main.cpp resources/resources.qrc resources/app.rc)
  add_executable(LiveStudio WIN32 ${LS_APP_SOURCES})
  set_source_files_properties(resources/app.rc PROPERTIES
    COMPILE_DEFINITIONS
    "LIVESTUDIO_VER_MAJOR=${PROJECT_VERSION_MAJOR};LIVESTUDIO_VER_MINOR=${PROJECT_VERSION_MINOR};LIVESTUDIO_VER_PATCH=${PROJECT_VERSION_PATCH};LIVESTUDIO_VERSION_STR=\"${PROJECT_VERSION}\"")
else()
  # Hanya untuk pemeriksaan kompilasi/uji di Linux. Produk yang didukung = Windows.
  target_compile_options(livestudio_core PUBLIC -Wall -Wextra -Wno-unused-parameter)
  add_executable(LiveStudio src/main.cpp resources/resources.qrc)
endif()

target_link_libraries(LiveStudio PRIVATE livestudio_core)
target_compile_definitions(LiveStudio PRIVATE LIVESTUDIO_VERSION="${PROJECT_VERSION}")

# Tata letak mengikuti OBS: <root>/bin/64bit/LiveStudio.exe, <root>/obs-plugins, <root>/data
set_target_properties(LiveStudio PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stage/bin/64bit")

install(TARGETS LiveStudio RUNTIME DESTINATION bin/64bit)
install(FILES LICENSE.txt THIRD_PARTY_NOTICES.md DESTINATION .)

# Runtime OBS (DLL libobs, obs-plugins, data) disalin bila folder sumbernya diberikan.
# Modul yang membutuhkan obs-frontend-api dikecualikan karena Live Studio tidak menyertakan UI OBS.
if(WIN32 AND LIVESTUDIO_OBS_RUNTIME_DIR)
  install(DIRECTORY "${LIVESTUDIO_OBS_RUNTIME_DIR}/bin/64bit/" DESTINATION bin/64bit
          FILES_MATCHING PATTERN "*.dll" PATTERN "*.exe"
          PATTERN "obs64.exe" EXCLUDE PATTERN "obs-frontend-api.dll" EXCLUDE)
  install(DIRECTORY "${LIVESTUDIO_OBS_RUNTIME_DIR}/obs-plugins/" DESTINATION obs-plugins
          PATTERN "frontend-tools.dll" EXCLUDE PATTERN "obs-websocket.dll" EXCLUDE
          PATTERN "*.pdb" EXCLUDE)
  install(DIRECTORY "${LIVESTUDIO_OBS_RUNTIME_DIR}/data/" DESTINATION data
          PATTERN "obs-plugins/frontend-tools" EXCLUDE PATTERN "obs-plugins/obs-websocket" EXCLUDE)
endif()

# Qt runtime: windeployqt dijalankan oleh scripts/package-windows.ps1 setelah install.

# ---- uji (opsional) ----
option(LIVESTUDIO_BUILD_TESTS "Bangun uji smoke GUI (butuh runtime OBS + display)" OFF)
if(LIVESTUDIO_BUILD_TESTS)
  add_executable(LiveStudioSmoke tests/smoke_gui.cpp resources/resources.qrc)
  target_link_libraries(LiveStudioSmoke PRIVATE livestudio_core)
  target_compile_definitions(LiveStudioSmoke PRIVATE LIVESTUDIO_VERSION="${PROJECT_VERSION}")
  set_target_properties(LiveStudioSmoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stage/bin/64bit")
endif()
