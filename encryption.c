#include <string.h>
#include <stdlib.h>
#include "encryption.h"

// XOR key - should be kept secret
static const char XOR_KEY[] = "MySecretKey123!";

void xor_encrypt(const char *input, char *output, size_t len) {
    size_t key_len = strlen(XOR_KEY);
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ XOR_KEY[i % key_len];
    }
}

void xor_decrypt(const char *input, char *output, size_t len) {
    // XOR encryption is symmetric, so we can use the same function for decryption
    xor_encrypt(input, output, len);
} 