cmake_minimum_required(VERSION 3.20)
project(SecureRdp VERSION 1.0.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
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
    src/MRemoteNgExchange.cpp
    src/CredentialVault.cpp
    src/RdpLauncher.cpp
    src/Clipboard.cpp
    src/DragDrop.cpp
    src/Util.cpp
    src/Logger.cpp
    src/UiTheme.cpp
    src/ThemeSettings.cpp
    src/Dialogs/SessionDialog.cpp
    src/Dialogs/FolderDialog.cpp
    src/Dialogs/CredentialDialog.cpp
    src/Dialogs/AboutDialog.cpp
    src/Dialogs/ChangelogDialog.cpp
    src/Dialogs/AssignCredentialDialog.cpp
    src/Dialogs/FolderScopeDialog.cpp
    src/CredentialUi.cpp
    resources/app.rc
)

set(BUILD_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UpdateBuildVersion.cmake")
set(BUILD_VERSION_COUNTER "${CMAKE_CURRENT_SOURCE_DIR}/.build-revision")
set(BUILD_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildVersion.h")

add_custom_target(update_build_version ALL
    COMMAND ${CMAKE_COMMAND}
        -DCOUNTER_FILE=${BUILD_VERSION_COUNTER}
        -DOUTPUT_HEADER=${BUILD_VERSION_HEADER}
        -DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DVERSION_MINOR=${PROJECT_VERSION_MINOR}
        -DVERSION_PATCH=${PROJECT_VERSION_PATCH}
        -P ${BUILD_VERSION_SCRIPT}
    BYPRODUCTS "${BUILD_VERSION_HEADER}"
    COMMENT "Updating build revision"
    VERBATIM
)

add_executable(SecureRdp WIN32 ${SOURCES})
add_dependencies(SecureRdp update_build_version)

# Rebuild resources when the icon changes.
set_source_files_properties(resources/app.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico;${CMAKE_CURRENT_SOURCE_DIR}/resources/app-source-no-background.png;${CMAKE_CURRENT_SOURCE_DIR}/resources/changelog.txt"
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
    dwmapi
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
