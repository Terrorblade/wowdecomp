#include "fmod_string.h"
#include "fmod_stringw.h"
#include "fmod_memory.h"

// VERIFY: e2386f82 auto
// OFFSET: 0x0091CCC0
int FMOD_strlenW(const short * s)
{
    const short *sc;

    for (sc = s; *sc != '\0'; ++sc) /* nothing */;
    
    return (int)(sc - s);
}

short * FMOD_strcpyW(short * dest,const short *src)
{
    short *tmp = dest;

    while ((*dest++ = *src++) != '\0') /* nothing */;
    
    return tmp;
}

// VERIFY: 041be19c auto
// OFFSET: 0x0091CCE0
short * FMOD_strncpyW(short * dest, const short *src, int count)
{
    short *tmp = dest;

    while (count-- && (*dest++ = *src++) != '\0') /* nothing */;

    return tmp;
}


short * FMOD_strcatW(short * dest, const short * src)
{
    short *tmp = dest;

    while (*dest)   dest++;
    while ((*dest++ = *src++) != '\0') ;

    return tmp;
}

short * FMOD_strncatW(short *dest, const short *src, int count)
{
    short *tmp = dest;

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

short FMOD_tolowerW(short in)
{
    if (in >= 'A' && in <= 'Z')
    {
        in += 'a' - 'A';
    }

    return in;
}

short *FMOD_struprW(short *string)
{
    short * cp;

    for (cp=string; *cp; ++cp)
    {
        if (*cp >= 'a' && *cp <= 'z')
        {
            *cp += 'A' - 'a';
        }
    }

    return(string);
}

short *FMOD_strlwrW(short *string)
{
    short * cp;

    for (cp=string; *cp; ++cp)
    {
        if (*cp >= 'A' && *cp <= 'Z')
        {
            *cp += 'a' - 'A';
        }
    }

    return(string);
}


int FMOD_strcmpW( const short *string1, const short *string2 )
{
    short c1, c2;
    do 
    {
        c1 = *string1++;
        c2 = *string2++;
    } while ( c1 && (c1 == c2) );

    return c1 - c2;
}


int FMOD_strncmpW( const short *string1, const short *string2, int len )
{
    short c1, c2;
    int count=0;
    do 
    {
        c1 = *string1++;
        c2 = *string2++;
        count++;
    } while ( c1 && (c1 == c2) && count < len);

    return c1 - c2;
}


int FMOD_stricmpW( const short *string1, const short *string2 )
{
    short c1, c2;
    do 
    {
        c1 = FMOD_tolowerW(*string1++);
        c2 = FMOD_tolowerW(*string2++);
    } while ( c1 && (c1 == c2) );

    return c1 - c2;
}

int FMOD_strnicmpW( const short *string1, const short *string2, int len )
{
    short c1, c2;
    int count=0;
    do 
    {
        c1 = FMOD_tolowerW(*string1++);
        c2 = FMOD_tolowerW(*string2++);
        count++;
    } while ( c1 && (c1 == c2) && count < len);

    return c1 - c2;
}

short * FMOD_strstrW(const short * s1,const short * s2)
{
    int l1, l2;

    l2 = FMOD_strlenW(s2);

    if (!l2)
    {
        return (short *) s1;
    }
    
    l1 = FMOD_strlenW(s1);
    
    while (l1 >= l2) 
    {
        l1--;

        if (!FMOD_memcmp(s1,s2,l2)) return (short *) s1;

        s1++;
    }
    return (short *)0;
}

short *FMOD_strdupW(const short * src)
{
    short *ret = 0;

    ret = (short *)FMOD_Memory_Alloc(FMOD_strlenW(src) + 2);
    if (ret)
    {
        FMOD_strcpyW(ret, src);
    }

    return ret;
}


short *FMOD_eatwhiteW(const short *string)
{
    for (;*string && FMOD_isspaceW(*string);string++) ;

    return (short *)string;
}


/*
    AJS This only handles decimal integers. Feel free to extend it if you need to
*/
int FMOD_atoiW(const short *s)
{
    if (!s)
    {
        return 0;
    }

    int   val      = 0;
    const short *p = s;

    for (; *p; p++) ;

    p--;
    for (int i=1; p >= s; p--, i *= 10)
    {
        val += ((*p - '0') * i);
    }

    return val;
}


char *FMOD_wtoa(short *s)
{
    if (!s)
    {
        return NULL;
    }

    char *from  = (char *)s;
    char *to    = (char *)s;
    
    while (*from != '\0')
    {
        *to   = *from;
        to   += 1;
        from += 2;
    }
    *to = 0;
    
    return (char *)s;
}
