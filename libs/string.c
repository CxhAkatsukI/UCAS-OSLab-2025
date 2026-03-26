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
    // 1. Input validation: Check for valid buffer and base.
    if (buffer == NULL || (base != 10 && base != 16)) {
        return NULL;
    }

    // 2. Handle the edge case of 0 separately.
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    // Use an unsigned integer for the conversion logic to correctly handle INT_MIN.
    // If value is INT_MIN, -value will overflow a signed int.
    unsigned int n = value;
    bool is_negative = false;

    // Handle negative numbers only for base 10.
    // Hexadecimal is typically represented as unsigned.
    if (value < 0 && base == 10) {
        is_negative = true;
        n = -value;
    }

    // Build the string in reverse order in a temporary buffer.
    char temp_buf[33]; // 32 bits + null terminator is safe for any int.
    int i = 32;
    temp_buf[i--] = '\0'; // Start with the null terminator at the very end.

    // 3. Main conversion loop.
    while (n > 0) {
        int remainder = n % base;

        // **KEY CHANGE**: Handle hexadecimal characters 'a' through 'f'.
        if (remainder >= 10) {
            temp_buf[i--] = (remainder - 10) + 'a';
        } else {
            temp_buf[i--] = remainder + '0';
        }
        n = n / base;
    }

    // 4. If the number was negative (in base 10), add the '-' sign.
    if (is_negative) {
        temp_buf[i--] = '-';
    }

    // 5. Copy the reversed string from temp_buf to the final buffer.
    // i+1 is the starting position of our string in temp_buf.
    char *p_temp = &temp_buf[i + 1];
    char *p_buffer = buffer;
    while (*p_temp) {
        *p_buffer++ = *p_temp++;
    }
    *p_buffer = '\0'; // Add the final null terminator.

    return buffer;
}
