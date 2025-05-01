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