cmake_minimum_required(VERSION 3.1)

project(TftpClient LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

file (GLOB SRC src/*.cpp)

if (WIN32)
    add_executable(${PROJECT_NAME} WIN32 "${SRC}" "qml.qrc" "${CMAKE_SOURCE_DIR}/img/app.rc")
else()
    add_executable(${PROJECT_NAME} "${SRC}" "qml.qrc")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Quick)

# ---------------------------------------------------------------
# Installation
#
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION "0.3")
set(CPACK_PACKAGE_VENDOR "TODO")
set(CPACK_PACKAGE_CONTACT "TODO")
set(CPACK_PACKAGE_DESCRIPTION "TFTP Client")
set(CPACK_STRIP_FILES ON)
if (WIN32)
    find_program(WINDEPLOYQT windeployqt PATHS ${QT5_ROOT_PATH}/bin/)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

    if (PACKMSI)
        set(CPACK_GENERATOR NSIS)
    else ()
        set(CPACK_GENERATOR ZIP)
    endif ()
    set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_NAME}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/img/logo.ico)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${PROJECT_NAME}.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "TODO")
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

    add_custom_target(windeployqt ALL
        ${WINDEPLOYQT}
        --dir ${PROJECT_BINARY_DIR}/deploy
        --release
        --compiler-runtime
        --qmldir ${PROJECT_SOURCE_DIR}/qml
        $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        COMMENT "Preparing Qt runtime dependencies")
    install(DIRECTORY ${PROJECT_BINARY_DIR}/deploy/ DESTINATION .)

    IF(CMAKE_CL_64)
    SET(CMAKE_MSVC_ARCH x64)
    ELSE(CMAKE_CL_64)
    SET(CMAKE_MSVC_ARCH x86)
    ENDIF(CMAKE_CL_64)

    FIND_PROGRAM(MSVC_REDIST
        NAMES vcredist_${CMAKE_MSVC_ARCH}.exe
        PATHS ${PROJECT_BINARY_DIR}/deploy/)
    GET_FILENAME_COMPONENT(vcredist_name "${MSVC_REDIST}" NAME)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\"$INSTDIR\\\\vcredist_${CMAKE_MSVC_ARCH}.exe\\\" /install /quiet /norestart'")

else ()
    message (CRITICAL "Unsupported OS")
endif ()

include(CPack)
