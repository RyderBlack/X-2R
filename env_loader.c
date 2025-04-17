#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define set_env _putenv
#else
#include <unistd.h>
#define set_env setenv_wrapper
int setenv_wrapper(const char* key, const char* value, int overwrite) {
    if (!overwrite && getenv(key)) return 0;
    return setenv(key, value, 1);
}
#endif

void load_env_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Could not open .env file");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Trim newline
        line[strcspn(line, "\r\n")] = 0;

        // Skip empty or comment lines
        if (line[0] == '\0' || line[0] == '#') continue;

        // Find '=' and split key/value
        char* equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        // DEBUG: Print loaded key-value pairs
        // printf("Loaded env: %s = %s\n", key, value);

#ifdef _WIN32
        // Windows uses a single string "KEY=value" for _putenv
        char env_string[512];
        snprintf(env_string, sizeof(env_string), "%s=%s", key, value);
        _putenv(env_string);
#else
        setenv(key, value, 1);
#endif
    }

    fclose(file);
}
