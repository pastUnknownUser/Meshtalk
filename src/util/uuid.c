#include "../common.h"
#include "uuid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <bcrypt.h>
    static void random_bytes(uint8_t* buf, size_t len) {
        BCRYPT_ALG_HANDLE h;
        BCryptOpenAlgorithmProvider(&h, BCRYPT_RNG_ALGORITHM, NULL, 0);
        BCryptGenRandom(h, buf, (ULONG)len, 0);
        BCryptCloseAlgorithmProvider(h, 0);
    }
#else
    #include <fcntl.h>
    #include <unistd.h>
    static void random_bytes(uint8_t* buf, size_t len) {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            read(fd, buf, len);
            close(fd);
        }
    }
#endif

void uuid_generate(char* out_str) {
    uint8_t bytes[16];
    random_bytes(bytes, 16);

    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;

    snprintf(out_str, UUID_STR_LEN + 1,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11],
        bytes[12], bytes[13], bytes[14], bytes[15]);
}

bool uuid_validate(const char* str) {
    if (!str) return false;
    size_t len = strlen(str);
    if (len != 36) return false;
    for (size_t i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') return false;
        } else {
            if (!((str[i] >= '0' && str[i] <= '9') ||
                  (str[i] >= 'a' && str[i] <= 'f') ||
                  (str[i] >= 'A' && str[i] <= 'F')))
                return false;
        }
    }
    return true;
}
