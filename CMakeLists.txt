cmake_minimum_required(VERSION 3.22)
project(ChessAI)

set(CMAKE_CXX_STANDARD 20)

# --- Enable Qt meta-object compilation ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include headers
include_directories(include gui)

# Gather all core engine source files
file(GLOB SOURCES "src/*.cpp")

# --- Console version ---
add_executable(ChessAI ${SOURCES} main.cpp)

# --- GUI version using Qt6 Widgets ---
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# GUI sources
set(GUI_SOURCES
    gui/ChessBoardWidget.cpp
    gui/MainWindow.cpp
    gui/main_qt.cpp
)

add_executable(ChessAI_GUI
    ${SOURCES}           # engine code
    ${GUI_SOURCES}       # GUI code
)

# Include paths
target_include_directories(ChessAI_GUI PRIVATE include gui)

# Link Qt Widgets
target_link_libraries(ChessAI_GUI PRIVATE Qt6::Widgets)

# macOS app bundle
if(APPLE)
    set_target_properties(ChessAI_GUI PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.alfiecorthine.chessai"
        MACOSX_BUNDLE_BUNDLE_NAME "ChessAI"
    )
endif()
