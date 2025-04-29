#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

#define BUFFER_SIZE 1024

void trim_whitespace(char* str);
int string_compare(const char* str1, const char* str2);

#endif // STRING_UTILS_H 