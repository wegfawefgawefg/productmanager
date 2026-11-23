cmake_minimum_required(VERSION 3.25)
project(productmanager VERSION 0.3.0 LANGUAGES C CXX)

include(FetchContent)
include(GNUInstallDirs)

# SDL the same way as wosd: an installed SDL 3.4 if there is one, else the
# pinned source linked statically
find_package(SDL3 3.4 CONFIG QUIET)
if(NOT SDL3_FOUND)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
endif()

add_executable(productmanager
    src/main.cpp src/paths.cpp src/strip_app.cpp src/view_app.cpp src/views.cpp src/wm.cpp
    src/sessions.cpp src/transcript.cpp src/focus.cpp
    src/tiles.cpp src/big_view.cpp src/crew_card.cpp src/tile_layout.cpp
    src/icons.cpp src/shapes.cpp src/text.cpp src/thumbs.cpp src/capture.cpp)
target_compile_features(productmanager PRIVATE cxx_std_20)
target_include_directories(productmanager PRIVATE src)
target_include_directories(productmanager SYSTEM PRIVATE third_party)
target_compile_definitions(productmanager
    PRIVATE PM_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_compile_options(productmanager PRIVATE -Wall -Wextra)
target_link_libraries(productmanager PRIVATE SDL3::SDL3)

install(TARGETS productmanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/productmanager)
# pm: the short name
install(CODE "file(CREATE_LINK productmanager
    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/pm\" SYMBOLIC)")
