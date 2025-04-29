cmake_minimum_required(VERSION 3.10)
project(X_RR C)

set(CMAKE_C_STANDARD 23)

# --- Setup for GTK3 ---
find_package(PkgConfig REQUIRED)

# Check if GTK3 is available
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Include GTK3 directories
include_directories(${GTK3_INCLUDE_DIRS})

# --- PostgreSQL Setup ---
if (WIN32)
    set(POSTGRESQL_ROOT "C:/Program Files/PostgreSQL/17")
    include_directories("${POSTGRESQL_ROOT}/include")
    link_directories("${POSTGRESQL_ROOT}/lib")
endif()

# --- Common Source Files ---
set(COMMON_SOURCES
        env_loader.c
        platform.h
        protocol.c
        protocol.h
        db_connection.c
        db_connection.h
        encryption.c
)

# --- Server Target ---
add_executable(server
        main_server.c
        ${COMMON_SOURCES}
        chat_window.c
        chat_window.h
)

# --- Client Target ---
add_executable(client
        main_client.c
        ${COMMON_SOURCES}
)

# --- GTK App Target ---
add_executable(gtk_app
        gtk_app.c
        ${COMMON_SOURCES}
)

# --- Link Libraries ---
if (WIN32)
    target_link_libraries(server libpq ws2_32)
    target_link_libraries(client ws2_32)

    # GTK App needs GTK, Winsock, pthread, and PostgreSQL
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    include_directories(${GTK3_INCLUDE_DIRS})
    target_link_libraries(gtk_app ${GTK3_LIBRARIES} ws2_32 pthread libpq)
else()
    target_link_libraries(server pq)
    target_link_libraries(client)

    # GTK App needs GTK, pthread, and PostgreSQL
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    include_directories(${GTK3_INCLUDE_DIRS})
    target_link_libraries(gtk_app ${GTK3_LIBRARIES} pthread pq)
endif()

# Link GTK3 libraries after the targets are created
target_link_libraries(client ${GTK3_LIBRARIES})
target_link_libraries(server ${GTK3_LIBRARIES})

# --- Copy PostgreSQL DLLs (Windows only) ---
if (WIN32)
    set(DLL_FILES
            "${POSTGRESQL_ROOT}/bin/libpq.dll"
            "${POSTGRESQL_ROOT}/bin/libssl-3-x64.dll"
            "${POSTGRESQL_ROOT}/bin/libcrypto-3-x64.dll"
            "${POSTGRESQL_ROOT}/bin/libiconv-2.dll"
            "${POSTGRESQL_ROOT}/bin/libintl-9.dll"
            "${POSTGRESQL_ROOT}/bin/zlib1.dll"
    )

    foreach(DLL ${DLL_FILES})
        message(STATUS "Looking for DLL: ${DLL}")
        add_custom_command(TARGET server POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" $<TARGET_FILE_DIR:server>
        )
        # Copy DLLs for GTK app
        add_custom_command(TARGET gtk_app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" $<TARGET_FILE_DIR:gtk_app>
        )
    endforeach()
endif()
