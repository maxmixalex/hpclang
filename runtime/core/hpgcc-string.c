#include <hpstring.h>
#include <limits.h>

char *uints(unsigned value, int base)
{
    static char output[sizeof(unsigned) * CHAR_BIT + 1];
    unsigned count = 0;
    if (base < 2 || base > 36)
        value = 0;
    do {
        unsigned digit = value ? value % (unsigned)base : 0;
        output[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        if (value)
            value /= (unsigned)base;
    } while (value);
    output[count] = '\0';
    for (unsigned i = 0; i < count / 2; ++i) {
        char saved = output[i];
        output[i] = output[count - i - 1];
        output[count - i - 1] = saved;
    }
    return output;
}

char *ints(int value, int base)
{
    if (value >= 0)
        return uints((unsigned)value, base);
    static char output[sizeof(unsigned) * CHAR_BIT + 2];
    /* Unsigned subtraction is defined even when value is INT_MIN. */
    const char *digits = uints(0u - (unsigned)value, base);
    output[0] = '-';
    memcpy(output + 1, digits, strlen(digits) + 1);
    return output;
}
