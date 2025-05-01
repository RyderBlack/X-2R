#include "gtk_string_utils.h"
#include <string.h>
#include <glib.h>
#include <stdlib.h>
#include <ctype.h>
#include "string_utils.h"

// Function to sanitize UTF-8 strings for Pango
const char* sanitize_utf8(const char* input) {
    static char sanitized[1024];
    if (!input) return NULL;

    if (!g_utf8_validate(input, -1, NULL)) {
        // If invalid UTF-8, replace invalid sequences
        size_t i = 0, j = 0;
        while (input[i] != '\0' && j < sizeof(sanitized) - 1) {
            if ((input[i] & 0x80) == 0) {
                sanitized[j++] = input[i++];
            } else if ((input[i] & 0xE0) == 0xC0 && (input[i+1] & 0xC0) == 0x80) {
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
            } else if ((input[i] & 0xF0) == 0xE0 && (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80) {
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
            } else {
                sanitized[j++] = '?';
                i++;
            }
        }
        sanitized[j] = '\0';
        return sanitized;
    }
    return input;
}

// Helper to sanitize UTF-8 strings for GTK display
const char* sanitize_utf8_gtk(const char *str) {
    if (!str || !g_utf8_validate(str, -1, NULL)) {
        return "[Invalid UTF-8]";
    }
    return str;
}