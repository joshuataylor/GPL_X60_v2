#include <common.h>
#include <linux/ctype.h>

#include "sscanf.c"

/* The following function(strtok_r) is copied from glibc.
 * We need the following utils to parsing our partition-table while we are handling
 * the firmware recovery stuffs.
 * Added by JeromeWang, 2014-09-10 */


/* Parse S into tokens separated by characters in DELIM.
   If S is NULL, the saved pointer in SAVE_PTR is used as
   the next starting point.  For example:
    char s[] = "-abc-=-def";
    char *sp;
    x = strtok_r(s, "-", &sp);	// x = "abc", sp = "=-def"
    x = strtok_r(NULL, "-=", &sp);	// x = "def", sp = NULL
    x = strtok_r(NULL, "=", &sp);	// x = NULL
        // s = "abc\0-def\0"
*/

char *strtok_r (char *s, const char *delim, char **save_ptr)
{
    char *token;

    if (s == NULL)
    {
        s = *save_ptr;
        if (s == NULL)
        {
            return NULL;
        }
    }

    /* Scan leading delimiters.  */
    s += strspn (s, delim);
    if (*s == '\0')
    {
        *save_ptr = s;
        return NULL;
    }

    /* Find the end of the token.  */
    token = s;
    s = strpbrk (token, delim);
    if (s == NULL)
    /* This token finishes the string.  */
    *save_ptr = strchr(token, '\0');
    else
    {
        /* Terminate the token and make *SAVE_PTR point past it.  */
        *s = '\0';
        *save_ptr = s + 1;
    }
    return token;
}

