#include <stdarg.h>
#include <stdio.h>

#include "fmod_string.h"
#include "fmod_memory.h"

using namespace FMOD;
     
// VERIFY: 9737ad22 auto
// OFFSET: 0x008D22C0
int FMOD_strlen(const char * s)
{
    const char *sc;

    for (sc = s; *sc != '\0'; ++sc) /* nothing */;
    
    return (int)(sc - s);
}

// VERIFY: 0d32f33c auto
// OFFSET: 0x008D22E0
char * FMOD_strcpy(char * dest,const char *src)
{
    char *tmp = dest;

    while ((*dest++ = *src++) != '\0') /* nothing */;
    
    return tmp;
}

// VERIFY: 93064d60 auto
// OFFSET: 0x008D2310
char * FMOD_strncpy(char * dest, const char *src, int count)
{
    char *tmp = dest;

    while (count-- && (*dest++ = *src++) != '\0') /* nothing */;

    return tmp;
}


// VERIFY: bce8eee8 auto
// OFFSET: 0x008D2340
char * FMOD_strcat(char * dest, const char * src)
{
    char *tmp = dest;

    while (*dest)   dest++;
    while ((*dest++ = *src++) != '\0') ;

    return tmp;
}

// VERIFY: 39c2d3d4 auto
// OFFSET: 0x008D2380
char * FMOD_strncat(char *dest, const char *src, int count)
{
    char *tmp = dest;

    if (count) 
    {
        while (*dest) dest++;
        
        while ((*dest++ = *src++)) 
        {
            if (--count == 0) 
            {
                *dest = '\0';
                break;
            }
        }
    }

    return tmp;
}

char FMOD_tolower(char in)
{
    if (in >= 'A' && in <= 'Z')
    {
        in += 'a' - 'A';
    }

    return in;
}

// VERIFY: 81f309bf manual
// OFFSET: 0x008D23D0
char *FMOD_strupr(char *string)
{
    char * cp;

    for (cp=string; *cp; ++cp)
    {
        if (*cp >= 'a' && *cp <= 'z')
        {
            *cp += 'A' - 'a';
        }
    }

    return(string);
}

char *FMOD_strlwr(char *string)
{
    char * cp;

    for (cp=string; *cp; ++cp)
    {
        if (*cp >= 'A' && *cp <= 'Z')
        {
            *cp += 'a' - 'A';
        }
    }

    return(string);
}

// VERIFY: 9634cfc4 manual
// OFFSET: 0x008D2400
int FMOD_strcmp( const char *string1, const char *string2 )
{
    char c1, c2;
    do 
    {
        c1 = *string1++;
        c2 = *string2++;
    } while ( c1 && (c1 == c2) );

    return c1 - c2;
}


// VERIFY: 80cb4eda manual
// OFFSET: 0x008D2430
int FMOD_strncmp( const char *string1, const char *string2, int len )
{
    char c1, c2;
    int count=0;
    do 
    {
        c1 = *string1++;
        c2 = *string2++;
        count++;
    } while ( c1 && (c1 == c2) && count < len);

    return c1 - c2;
}


// VERIFY: c95b4aab auto
// OFFSET: 0x008D2470
int FMOD_stricmp( const char *string1, const char *string2 )
{
    char c1, c2;
    do 
    {
        c1 = FMOD_tolower(*string1++);
        c2 = FMOD_tolower(*string2++);
    } while ( c1 && (c1 == c2) );

    return c1 - c2;
}

// VERIFY: f52be387 auto
// OFFSET: 0x008D24C0
int FMOD_strnicmp( const char *string1, const char *string2, int len )
{
    char c1, c2;
    int count=0;
    do 
    {
        c1 = FMOD_tolower(*string1++);
        c2 = FMOD_tolower(*string2++);
        count++;
    } while ( c1 && (c1 == c2) && count < len);

    return c1 - c2;
}

char * FMOD_strchr(const char * s1, int c)
{
    int l1;
    
    l1 = FMOD_strlen(s1);
    
    while (l1) 
    {
        l1--;

        if (*s1 == c) return (char *) s1;

        s1++;
    }
    return (char *)0;
}

// VERIFY: 09b83d75 auto
// OFFSET: 0x008D2600
char * FMOD_strstr(const char * s1,const char * s2)
{
    int l1, l2;

    l2 = FMOD_strlen(s2);

    if (!l2)
    {
        return (char *) s1;
    }
    
    l1 = FMOD_strlen(s1);
    
    while (l1 >= l2) 
    {
        l1--;

        if (!FMOD_memcmp(s1,s2,l2)) return (char *) s1;

        s1++;
    }
    return (char *)0;
}


int FMOD_memcmp(const void * cs,const void * ct, int count)
{
    const unsigned char *su1, *su2;
    signed char res = 0;

    for( su1 = (const unsigned char *)cs, su2 = (const unsigned char *)ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}


// VERIFY: 2aac1756 manual
// OFFSET: 0x008D2510
void * FMOD_memmove(void * dest,const void *src, int count)
{
    char *tmp, *s;

    if (dest <= src) 
    {
        tmp = (char *) dest;
        s = (char *) src;
        while (count--) *tmp++ = *s++;
    }
    else 
    {
        tmp = (char *) dest + count;
        s = (char *) src + count;
        while (count--) *--tmp = *--s;
    }

    return dest;
}


// VERIFY: 2b97ba68 auto
// OFFSET: 0x008D2570
char *FMOD_strdup(const char * src)
{
    char *ret = 0;

    ret = (char *)FMOD_Memory_Alloc(FMOD_strlen(src) + 1);
    if (ret)
    {
        FMOD_strcpy(ret, src);
    }

    return ret;
}


// VERIFY: 309d4fa3 auto
// OFFSET: 0x008D25D0
char *FMOD_eatwhite(const char *string)
{
    for (;*string && FMOD_isspace(*string);string++) ;

    return (char *)string;
}


/*
    AJS This only handles decimal integers. Feel free to extend it if you need to
*/
int FMOD_atoi(const char *s)
{
    if (!s)
    {
        return 0;
    }

    int   val     = 0;
    const char *p = s;

    for (; *p; p++) ;

    p--;
    for (int i=1; p >= s; p--, i *= 10)
    {
        val += ((*p - '0') * i);
    }

    return val;
}

int FMOD_atoi_hex(const char *s)
{
    if (!s)
    {
        return 0;
    }

    int val = 0;

    for(const char *p = s; *p; p++)
    {
        val *= 16;

        if(*p >= '0' && *p <= '9')
        {
            val += (*p - '0');
        }
        else if(*p >= 'A' && *p <= 'F')
        {
            val += (*p - 'A' + 10);
        }
        else if(*p >= 'a' && *p <= 'f')
        {
            val += (*p - 'a' + 10);
        }
    }

    return val;
}

int FMOD_snprintf(char *str, int size, const char *format, ...)
{
    va_list arglist;

    va_start(arglist, format);

    int result = FMOD_vsnprintf(str, size, format, arglist);

    va_end(arglist);

    return result;
}

int FMOD_vsnprintf(char *str, int size, const char *format, va_list arglist)
{
#ifdef PLATFORM_PS2_IOP
    int result = vsprintf(str, format, arglist);
#else
    int result = vsnprintf(str, size, format, arglist);
#endif

    str[size - 1] = 0;

    return result;
}
