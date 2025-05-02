#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "../config/env_loader.h"
#include "db_connection.h"

PGconn* connect_to_db() {
    const char *host = getenv("PG_HOST");
    const char *port = getenv("PG_PORT");
    const char *dbname = getenv("PG_DB");
    const char *user = getenv("PG_USER");
    const char *password = getenv("PG_PASSWORD");

    // If no environment variables are set, try loading from .env file
    if (!host && !port && !dbname && !user && !password) {
        printf("ℹ️ No environment variables found, attempting to load from .env file...\n");
        
        // Try current directory first
        if (!load_env(".env")) {
            // If not found, try parent directory
            if (!load_env("../.env")) {
                fprintf(stderr, "❌ Could not find .env file in current or parent directory\n");
                return NULL;
            }
        }
        
        // Reload environment variables after loading .env file
        host = getenv("PG_HOST");
        port = getenv("PG_PORT");
        dbname = getenv("PG_DB");
        user = getenv("PG_USER");
        password = getenv("PG_PASSWORD");
        
        printf("✅ Loaded environment variables from .env file\n");
    } else {
        printf("✅ Using environment variables from system\n");
    }

    printf("🔍 Database connection details:\n");
    printf("   Host: %s\n", host ? host : "not set");
    printf("   Port: %s\n", port ? port : "not set");
    printf("   Database: %s\n", dbname ? dbname : "not set");
    printf("   User: %s\n", user ? user : "not set");
    printf("   Password: %s\n", password ? "****" : "not set");

    if (!host || !port || !dbname || !user || !password) {
        fprintf(stderr, "❌ Missing one or more required environment variables:\n");
        if (!host) fprintf(stderr, "   - PG_HOST is missing\n");
        if (!port) fprintf(stderr, "   - PG_PORT is missing\n");
        if (!dbname) fprintf(stderr, "   - PG_DB is missing\n");
        if (!user) fprintf(stderr, "   - PG_USER is missing\n");
        if (!password) fprintf(stderr, "   - PG_PASSWORD is missing\n");
        return NULL;
    }

    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s sslmode=prefer",
             host, port, dbname, user, password);

    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "❌ Database connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    printf("✅ Connected to database successfully.\n");
    return conn;
}