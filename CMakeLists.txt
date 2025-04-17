cmake_minimum_required(VERSION 3.10)
project(X_RR C)

set(CMAKE_C_STANDARD 23)

# === PostgreSQL Setup ===
set(POSTGRESQL_ROOT "C:/Program Files/PostgreSQL/17")
include_directories("${POSTGRESQL_ROOT}/include")
link_directories("${POSTGRESQL_ROOT}/lib")


find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})


add_executable(X_RR
        main.c
        env_loader.c
        db_connection.c
)

target_link_libraries(X_RR
        ${GTK3_LIBRARIES}
        libpq
        ws2_32
)

# === Copy PostgreSQL DLLs ===
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
        add_custom_command(TARGET X_RR POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" $<TARGET_FILE_DIR:X_RR>
        )
    endforeach()
endif()
