#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "env_loader.h"
#include "db_connection.h"

PGconn* connect_to_db() {
    load_env(".env");

    const char *host = getenv("PG_HOST");
    const char *port = getenv("PG_PORT");
    const char *dbname = getenv("PG_DB");
    const char *user = getenv("PG_USER");
    const char *password = getenv("PG_PASSWORD");

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
             "host=%s port=%s dbname=%s user=%s password=%s",
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