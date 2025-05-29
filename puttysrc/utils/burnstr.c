/*
 * 'Burn' a dynamically allocated string, in the sense of destroying
 * it beyond recovery: overwrite it with zeroes and then free it.
 */

#include "defs.h"
#include "misc.h"

void burnstr(char *string)             /* sfree(str), only clear it first */
{
    if (string) {
        smemclr(string, strlen(string));
        sfree(string);
    }
}
