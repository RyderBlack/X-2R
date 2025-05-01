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

# --- Setup for GTK3 ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})

# --- PostgreSQL Setup ---
if(WIN32)
    set(POSTGRESQL_ROOT "C:/Program Files/PostgreSQL/17")
    include_directories("${POSTGRESQL_ROOT}/include")
    link_directories("${POSTGRESQL_ROOT}/lib")
    set(ENV{PKG_CONFIG_PATH} "C:/msys64/mingw64/lib/pkgconfig")
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

# Common source files for both client and server
set(COMMON_SOURCES
    src/network/protocol.c
    src/database/db_connection.c
    src/config/env_loader.c
    src/security/encryption.c
    src/utils/string_utils.c
)

# Common header files
set(COMMON_HEADERS
    src/network/protocol.h
    src/network/platform.h
    src/database/db_connection.h
    src/config/env_loader.h
    src/security/encryption.h
    src/utils/string_utils.h
)

# Server sources
set(SERVER_SOURCES
    src/main_server.c
)

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

# Create executables
add_executable(server ${SERVER_SOURCES} ${COMMON_SOURCES} ${COMMON_HEADERS})
add_executable(gtk_app ${GTK_APP_SOURCES} ${COMMON_SOURCES} ${GTK_ONLY_SOURCES}  ${COMMON_HEADERS})

# Include directories for all executables
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

# Link libraries
if(WIN32)
    target_link_libraries(server PRIVATE libpq ws2_32)
    target_link_libraries(gtk_app PRIVATE ${GTK3_LIBRARIES} ws2_32 pthread libpq)
else()
    target_link_libraries(server PRIVATE pq)
    target_link_libraries(gtk_app PRIVATE ${GTK3_LIBRARIES} pthread pq)
endif()

# Copy DLLs on Windows
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
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}" $<TARGET_FILE_DIR:server>
        )
        add_custom_command(TARGET gtk_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}" $<TARGET_FILE_DIR:gtk_app>
        )
    endforeach()


endif()

# Install CSS file
install(FILES src/styles/app.css
    DESTINATION share/${PROJECT_NAME}/styles
    PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
)

# Install executable
install(TARGETS server gtk_app
    RUNTIME DESTINATION bin
)

# Create a script to run the application with the correct CSS path
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/run_gtk_app.bat.in"
    "${CMAKE_CURRENT_BINARY_DIR}/run_gtk_app.bat"
    @ONLY
)

# Install the run script
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/run_gtk_app.bat"
    DESTINATION bin
)

# Add new sources
set(SOURCES
    src/main.c
    src/main_server.c
    src/components/chat_page.c
    src/components/login_page.c
    src/components/register_page.c
    src/config/env_loader.c
    src/database/db_connection.c
    src/network/protocol.c
    src/security/encryption.c
    src/utils/gtk_string_utils.c
    src/utils/string_utils.c
    src/utils/chat_utils.c
)

# Add application icon
install(FILES src/icon.png DESTINATION bin)

add_custom_command(TARGET gtk_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/icon.png" "${CMAKE_SOURCE_DIR}/icon.png"
)