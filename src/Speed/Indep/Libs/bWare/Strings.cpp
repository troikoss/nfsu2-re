#include "Strings.h"

unsigned int cshash(const char *str) {
    unsigned int hash = 0xFFFFFFFF;
    while (*str) {
        hash = hash * 33 + static_cast<unsigned char>(*str);
        ++str;
    }
    return hash;
}

unsigned int cshashWithStartValue(const char *str, unsigned int startValue) {
    while (*str) {
        startValue = startValue * 33 + static_cast<unsigned char>(*str);
        ++str;
    }
    return startValue;
}

int strlenWithNullcheck(const char *str) {
    int length = 0;
    if (str) {
        while (str[length]) {
            ++length;
        }
    }
    return length;
}

unsigned int cihash(const char *str) {
    unsigned int hash = 0xFFFFFFFF;
    if (str) {
        while (*str) {
            hash *= 33;
            char c = *str;
            if (c >= 'a' && c <= 'z') {
                c += 'A' - 'a';
            }
            hash += static_cast<unsigned char>(c);
            ++str;
        }
    }
    return hash;
}

char *nfs_strcpy(char *dest, const char *src) {
    int i = 0;
    while ((dest[i] = src[i]) != 0) {
        ++i;
    }
    return dest;
}

char *nfs_strncpy(char *dest, const char *src, int maxlen) {
    int i = 0;
    while (maxlen) {
        --maxlen;
        if ((dest[i] = src[i]) == 0) {
            break;
        }
        ++i;
    }
    return dest;
}

int nfs_strcmp(const char *a, const char *b) {
    char ca;
    char cb;
    do {
        ca = *a++;
        cb = *b++;
    } while (ca && cb && ca == cb);
    return ca - cb;
}

int nfs_stricmp(const char *a, const char *b) {
    char ca;
    char cb;
    do {
        ca = *a;
        if (ca >= 'a' && ca <= 'z') {
            ca &= 0x5F;
        }
        ++a;
        cb = *b;
        if (cb >= 'a' && cb <= 'z') {
            cb &= 0x5F;
        }
        ++b;
    } while (ca && cb && ca == cb);
    return ca - cb;
}

int nfs_strncmp(const char *a, const char *b, int maxlen) {
    while (maxlen-- && *a && *b && *a++ == *b++) {
    }
    if (maxlen >= 0) {
        if (*a) {
            return 1;
        }
        if (*b) {
            return -1;
        }
        // The original reads the preceding bytes when both cursors are at NUL,
        // even if the input strings were empty. Preserve that observed behavior.
        return a[-1] - b[-1];
    }
    return 0;
}

