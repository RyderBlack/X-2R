cmake_minimum_required(VERSION 3.10)
project(X-RR C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32)
    set(PLATFORM_SOURCES "")
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_SOURCES "")
    set(PLATFORM_LIBS "")
endif()

# --- Setup for GTK3 --- (and GLib)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GTK3_INCLUDE_DIRS})

# --- PostgreSQL Setup ---
if(WIN32)
    set(POSTGRESQL_ROOT "C:/Program Files/PostgreSQL/17")
    include_directories("${POSTGRESQL_ROOT}/include")
    link_directories("${POSTGRESQL_ROOT}/lib")
    set(ENV{PKG_CONFIG_PATH} "C:/msys64/mingw64/lib/pkgconfig")
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

# Common source files
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

# Server sources
set(SERVER_SOURCES src/main_server.c)

# GTK App sources
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

set(GTK_ONLY_SOURCES
        src/utils/gtk_string_utils.c
        src/utils/gtk_string_utils.h
)

# Executables
add_executable(server ${SERVER_SOURCES} ${COMMON_SOURCES} ${COMMON_HEADERS})
add_executable(gtk_app ${GTK_APP_SOURCES} ${COMMON_SOURCES} ${GTK_ONLY_SOURCES} ${COMMON_HEADERS})

# Include directories
target_include_directories(server PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${POSTGRESQL_INCLUDE_DIRS}
        C:/msys64/mingw64/include
)

target_include_directories(gtk_app PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${GTK3_INCLUDE_DIRS}
        ${POSTGRESQL_INCLUDE_DIRS}
        C:/msys64/mingw64/include
)

# Linking
if(WIN32)
    target_link_libraries(server PRIVATE libpq ws2_32)
    target_link_libraries(gtk_app PRIVATE ${GTK3_LIBRARIES} ws2_32 pthread libpq)
else()
    target_link_libraries(server PRIVATE pq)
    target_link_libraries(gtk_app PRIVATE ${GTK3_LIBRARIES} pthread pq)
endif()

# Copy PostgreSQL DLLs
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
        add_custom_command(TARGET server POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:server>)
        add_custom_command(TARGET gtk_app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:gtk_app>)
    endforeach()
endif()

# GTK DLLs
set(GTK_PATH "C:/msys64/mingw64/bin") # <- Added to pass into .bat
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
    add_custom_command(TARGET gtk_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:gtk_app>)
endforeach()

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "C:/msys64/mingw64/etc/gtk-3.0" $<TARGET_FILE_DIR:gtk_app>/etc/gtk-3.0)

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "C:/msys64/mingw64/share/icons" $<TARGET_FILE_DIR:gtk_app>/etc/gtk-3.0)

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "C:/msys64/mingw64/share/themes" $<TARGET_FILE_DIR:gtk_app>/etc/gtk-3.0)

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/src/styles/app.css" "$<TARGET_FILE_DIR:gtk_app>/app.css")

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/.env" "$<TARGET_FILE_DIR:gtk_app>/.env")

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/icon.png" "$<TARGET_FILE_DIR:gtk_app>/icon.png")

# Install CSS
install(FILES src/styles/app.css DESTINATION share/${PROJECT_NAME}/styles PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Install executables
install(TARGETS server gtk_app RUNTIME DESTINATION bin)

# Install icon
install(FILES src/icon.png DESTINATION bin)

# --- Configure run_gtk_app.bat ---
configure_file(
        "${CMAKE_SOURCE_DIR}/src/run_gtk_app.bat.in"
        "${CMAKE_BINARY_DIR}/run_gtk_app.bat"
        @ONLY
)

# Install run script
install(PROGRAMS "${CMAKE_BINARY_DIR}/run_gtk_app.bat" DESTINATION bin)

# Install PostgreSQL DLLs
install(FILES
        "${POSTGRESQL_ROOT}/bin/libpq.dll"
        "${POSTGRESQL_ROOT}/bin/libssl-3-x64.dll"
        "${POSTGRESQL_ROOT}/bin/libcrypto-3-x64.dll"
        "${POSTGRESQL_ROOT}/bin/libiconv-2.dll"
        "${POSTGRESQL_ROOT}/bin/libintl-9.dll"
        "${POSTGRESQL_ROOT}/bin/zlib1.dll"
        DESTINATION bin
)

# Install GTK DLLs
install(FILES
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
        DESTINATION bin
)


# Define variables for .bat substitution
set(GTK_PATH ${GTK_PATH})
set(CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR}) # already defined, but explicit

# Pass extra variables to configure_file
configure_file(
        "${CMAKE_SOURCE_DIR}/src/run_gtk_app.bat.in"
        "${CMAKE_BINARY_DIR}/run_gtk_app.bat"
        @ONLY
)


include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "X-RR")
set(CPACK_PACKAGE_VENDOR "Ryhverse Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "X-RR Chat Application")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "X-RR")
set(CPACK_PACKAGE_FILE_NAME "X-RR-1.0.0")

set(CPACK_GENERATOR "NSIS")

set(CPACK_NSIS_DISPLAY_NAME "X-RR")
set(CPACK_NSIS_PACKAGE_NAME "X-RR Installer")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/src/icon.ico") # optional icon
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\gtk_app.exe")      # sets start menu icon

#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
#set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.txt")

include(CPack)
