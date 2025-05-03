cmake_minimum_required(VERSION 3.10)
project(X-RR C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform-specific
if(WIN32)
    add_definitions(-D_WIN32)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS "")
endif()

# --- PostgreSQL setup ---
if(WIN32)
    if(NOT DEFINED POSTGRESQL_ROOT)
        set(POSTGRESQL_DEFAULT_PATH "C:/Program Files/PostgreSQL/17")
        if(EXISTS "${POSTGRESQL_DEFAULT_PATH}")
            set(POSTGRESQL_ROOT "${POSTGRESQL_DEFAULT_PATH}")
            message(STATUS "Using default PostgreSQL path: ${POSTGRESQL_ROOT}")
        else()
            message(FATAL_ERROR "POSTGRESQL_ROOT not set and default path not found. Please pass -DPOSTGRESQL_ROOT=<path>")
        endif()
    else()
        message(STATUS "Using user-defined POSTGRESQL_ROOT: ${POSTGRESQL_ROOT}")
    endif()

    include_directories("${POSTGRESQL_ROOT}/include")
    link_directories("${POSTGRESQL_ROOT}/lib")
endif()

# --- GTK setup ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(${GTK3_INCLUDE_DIRS})

if(WIN32)
    if(NOT DEFINED GTK_PATH)
        set(GTK_DEFAULT_PATH "C:/msys64/mingw64/bin")
        if(EXISTS "${GTK_DEFAULT_PATH}")
            set(GTK_PATH "${GTK_DEFAULT_PATH}")
            message(STATUS "Using default GTK path: ${GTK_PATH}")
        else()
            message(FATAL_ERROR "GTK_PATH not set and default path not found. Please pass -DGTK_PATH=<path>")
        endif()
    else()
        message(STATUS "Using user-defined GTK_PATH: ${GTK_PATH}")
    endif()
endif()

# --- Common sources ---
set(COMMON_SOURCES
        src/network/protocol.c
        src/database/db_connection.c
        src/config/env_loader.c
        src/security/encryption.c
        src/utils/string_utils.c
)

set(COMMON_HEADERS
        src/network/protocol.h
        src/network/platform.h
        src/database/db_connection.h
        src/config/env_loader.h
        src/security/encryption.h
        src/utils/string_utils.h
)

set(SERVER_SOURCES src/main_server.c)

set(GTK_APP_SOURCES
        src/main.c
        src/components/login_page.c
        src/components/register_page.c
        src/components/chat_page.c
        src/types/app_types.h
        src/components/login_page.h
        src/components/register_page.h
        src/components/chat_page.h
        src/utils/chat_utils.c
        src/utils/chat_utils.h
        src/utils/gtk_string_utils.c
        src/utils/gtk_string_utils.h
)

# --- Executables ---
add_executable(server ${SERVER_SOURCES} ${COMMON_SOURCES} ${COMMON_HEADERS})
add_executable(gtk_app ${GTK_APP_SOURCES} ${COMMON_SOURCES} ${COMMON_HEADERS})

# --- Includes ---
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${POSTGRESQL_INCLUDE_DIRS})
target_include_directories(gtk_app PRIVATE ${CMAKE_SOURCE_DIR} ${GTK3_INCLUDE_DIRS} ${POSTGRESQL_INCLUDE_DIRS})

# --- Linking ---
target_link_libraries(server PRIVATE libpq ${PLATFORM_LIBS})
target_link_libraries(gtk_app PRIVATE ${GTK3_LIBRARIES} libpq ${PLATFORM_LIBS} pthread)

# --- Copy PostgreSQL DLLs ---
if(WIN32)
    set(DLL_FILES
            "${POSTGRESQL_ROOT}/bin/libpq.dll"
            "${POSTGRESQL_ROOT}/bin/libssl-3-x64.dll"
            "${POSTGRESQL_ROOT}/bin/libcrypto-3-x64.dll"
            "${POSTGRESQL_ROOT}/bin/libiconv-2.dll"
            "${POSTGRESQL_ROOT}/bin/libintl-9.dll"
            "${POSTGRESQL_ROOT}/bin/zlib1.dll"
    )
    foreach(DLL ${DLL_FILES})
        add_custom_command(TARGET server POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" ${CMAKE_BINARY_DIR})
        add_custom_command(TARGET gtk_app POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" ${CMAKE_BINARY_DIR})
    endforeach()
endif()

# --- Copy GTK DLLs ---
if(WIN32)
    set(DLL_FILES_GTK
            "${GTK_PATH}/libgtk-3-0.dll"
            "${GTK_PATH}/libgdk-3-0.dll"
            "${GTK_PATH}/libatk-1.0-0.dll"
            "${GTK_PATH}/libcairo-2.dll"
            "${GTK_PATH}/libpango-1.0-0.dll"
            "${GTK_PATH}/libpangocairo-1.0-0.dll"
            "${GTK_PATH}/libgobject-2.0-0.dll"
            "${GTK_PATH}/libglib-2.0-0.dll"
            "${GTK_PATH}/libgmodule-2.0-0.dll"
            "${GTK_PATH}/libgthread-2.0-0.dll"
            "${GTK_PATH}/libffi-8.dll"
            "${GTK_PATH}/libepoxy-0.dll"
    )
    foreach(DLL ${DLL_FILES_GTK})
        add_custom_command(TARGET gtk_app POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" ${CMAKE_BINARY_DIR})
    endforeach()
endif()

# --- Copy GTK config dirs ---
add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/etc/gtk-3.0"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${GTK_PATH}/../etc/gtk-3.0" "${CMAKE_BINARY_DIR}/etc/gtk-3.0"
)

# --- Copy CSS ---
add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/src/styles/app.css" "${CMAKE_BINARY_DIR}/app.css"
)

# --- Copy .env ---
add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/.env" "${CMAKE_BINARY_DIR}/.env"
)

# --- Copy icon ---
add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/icon.png" "${CMAKE_BINARY_DIR}/icon.png"
)

# --- Configure .bat ---
configure_file("${CMAKE_SOURCE_DIR}/src/run_gtk_app.bat.in" "${CMAKE_BINARY_DIR}/run_gtk_app.bat" @ONLY)

# --- Install rules ---
install(TARGETS server gtk_app RUNTIME DESTINATION bin)
install(FILES src/styles/app.css DESTINATION share/${PROJECT_NAME}/styles)
install(FILES src/icon.png DESTINATION bin)
install(PROGRAMS "${CMAKE_BINARY_DIR}/run_gtk_app.bat" DESTINATION bin)

install(FILES ${DLL_FILES} DESTINATION bin)
install(FILES ${DLL_FILES_GTK} DESTINATION bin)

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "X-RR")
set(CPACK_PACKAGE_VENDOR "Ryhad Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "X-2R Chat Application")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "X-RR")
set(CPACK_PACKAGE_FILE_NAME "X-RR-Office_Edition-1.0.0")
set(CPACK_GENERATOR "NSIS")

set(CPACK_NSIS_DISPLAY_NAME "X-2R")
set(CPACK_NSIS_PACKAGE_NAME "X-2R Installer By The One and Only Ryhad (and Rimma!)")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/x2r_logo.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/gtk_app.exe")

include(CPack)
