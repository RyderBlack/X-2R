#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <stddef.h>

void xor_encrypt(const char *input, char *output, size_t len);
void xor_decrypt(const char *input, char *output, size_t len);

#endif // ENCRYPTION_H 