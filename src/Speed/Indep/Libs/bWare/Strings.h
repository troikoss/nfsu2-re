#ifndef BWARE_STRINGS_H
#define BWARE_STRINGS_H

// Provisional names from yugecin/nfsu2-re; GameCube symbols are not available.
extern "C" {
unsigned int cshash(const char *str);
unsigned int cshashWithStartValue(const char *str, unsigned int startValue);
unsigned int cihash(const char *str);
int strlenWithNullcheck(const char *str);
// Prefix avoids collisions with the host C runtime during port verification.
char *nfs_strcpy(char *dest, const char *src);
char *nfs_strncpy(char *dest, const char *src, int maxlen);
int nfs_strcmp(const char *a, const char *b);
int nfs_stricmp(const char *a, const char *b);
int nfs_strncmp(const char *a, const char *b, int maxlen);
}

#endif
