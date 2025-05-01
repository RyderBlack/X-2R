#include <string.h>
#include <glib.h>
#include <stdlib.h>
#include <ctype.h>
#include "string_utils.h"


void trim_whitespace(char* str) {
    if (!str) return;
    
    char* end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
}

int string_compare(const char* str1, const char* str2) {
    if (!str1 || !str2) return -1;
    return strcmp(str1, str2);
}

/* REMOVE THE ENTIRE linkify_text FUNCTION DEFINITION BELOW */
/*
// Function to find URLs in text and wrap them in Pango <a> tags
// Returns a newly allocated string that must be freed by the caller.
char* linkify_text(const char *text) {
    if (!text) return NULL;

    GString *result = g_string_new("");
    const char *p = text;
    const char *segment_start = text;

    while (*p) {
        const char *url_start = NULL;
        if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0) {
            url_start = p;
            const char *url_end = url_start;
            // Find the end of the URL (stop at whitespace or end of string)
            while (*url_end && !isspace((unsigned char)*url_end)) {
                url_end++;
            }

            // 1. Escape and append the text segment BEFORE the URL
            if (url_start > segment_start) {
                char *segment = g_strndup(segment_start, url_start - segment_start);
                char *escaped_segment = g_markup_escape_text(segment, -1);
                g_string_append(result, escaped_segment);
                g_free(escaped_segment);
                g_free(segment);
            }

            // 2. Append the URL wrapped in an <a> tag
            char *url = g_strndup(url_start, url_end - url_start);
            char *escaped_url = g_markup_escape_text(url, -1); // Escape URL for href attribute
            g_string_append_printf(result, "<a href='%s'>%s</a>", escaped_url, escaped_url);
            g_free(escaped_url);
            g_free(url);

            // Move p past the processed URL
            p = url_end;
            segment_start = p; // Start the next segment after the URL
        } else {
            p++; // Continue scanning
        }
    }

    // Append any remaining text after the last URL
    if (*segment_start) {
        char *escaped_segment = g_markup_escape_text(segment_start, -1);
        g_string_append(result, escaped_segment);
        g_free(escaped_segment);
    }

    return g_string_free(result, FALSE); // Return the owned string
}
*/ 