cmake_minimum_required(VERSION 3.20)
project(SecureRdp VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/App.cpp
    src/MainWindow.cpp
    src/ConnectionTreeModel.cpp
    src/JsonStore.cpp
    src/CredentialVault.cpp
    src/RdpLauncher.cpp
    src/Clipboard.cpp
    src/DragDrop.cpp
    src/Util.cpp
    src/Logger.cpp
    src/UiTheme.cpp
    src/Dialogs/SessionDialog.cpp
    src/Dialogs/FolderDialog.cpp
    src/Dialogs/CredentialDialog.cpp
    resources/app.rc
)

add_executable(SecureRdp WIN32 ${SOURCES})

# Rebuild resources when the icon changes.
set_source_files_properties(resources/app.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico"
)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")

target_include_directories(SecureRdp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(SecureRdp PRIVATE
    comctl32
    credui
    shell32
    ole32
    comdlg32
    shlwapi
    rpcrt4
    uxtheme
)

if(MSVC)
    target_link_options(SecureRdp PRIVATE
        /MANIFEST:NO
        $<$<CONFIG:Release>:/LTCG>
    )
    set_target_properties(SecureRdp PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
endif()

set_target_properties(SecureRdp PROPERTIES
    OUTPUT_NAME "SecureRdp"
    WIN32_EXECUTABLE TRUE
)
