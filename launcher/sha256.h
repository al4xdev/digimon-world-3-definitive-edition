#ifndef DW3_SHA256_H
#define DW3_SHA256_H

#include <stddef.h>
#include <stdint.h>

void sha256_bytes(const void *data, size_t len, char hex_out[65]);
int sha256_file(const char *path, char hex_out[65], size_t *size_out);

#endif
