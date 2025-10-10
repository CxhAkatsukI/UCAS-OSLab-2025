#include <os/string.h>

void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)
{
    for (; len != 0; len--) {
        *dest++ = *src++;
    }
}

void memset(void *dest, uint8_t val, uint32_t len)
{
    uint8_t *dst = (uint8_t *)dest;

    for (; len != 0; len--) {
        *dst++ = val;
    }
}

void bzero(void *dest, uint32_t len)
{
    memset(dest, 0, len);
}

int strlen(const char *src)
{
    int i = 0;
    while (src[i] != '\0') {
        i++;
    }
    return i;
}

int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

int strncmp(const char *str1, const char *str2, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i)
        if (str1[i] == '\0' || str1[i] != str2[i])
            return str1[i] - str2[i];
    return 0;
}


char *strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

char *strncpy(char *dest, const char *src, int n)
{
    char *tmp = dest;

    while (*src && n-- > 0) {
        *dest++ = *src++;
    }

    while (n-- > 0) {
        *dest++ = '\0';
    }

    return tmp;
}

char *strcat(char *dest, const char *src)
{
    char *tmp = dest;

    while (*dest != '\0') {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

// implement a simple itoa function to print loaded task num
char *itoa(int value, char *buffer, int base) {
    if (base != 10 || value > INT32_MAX)
        return NULL;
    if (value == 0) {
        *(buffer) = '0';
        *(buffer + 1) = '\0';
        return buffer;
    }
    char temp_buf[32];
    int temp_ptr = 31;
    while (value > 0) {
        temp_buf[temp_ptr] = value % base + '0';
        value = value / base;
        temp_ptr--;
    }
    temp_ptr++;
    char *copy_ptr = buffer;
    while (temp_ptr < 32) {
        *copy_ptr = temp_buf[temp_ptr];
        copy_ptr++;
        temp_ptr++;
    }
    *(copy_ptr) = '\0';
    return buffer;
}
