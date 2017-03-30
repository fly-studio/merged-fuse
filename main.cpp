#include <stdio.h>

#include "include/concat.h"
#include "include/common.h"

/*
int main()
{
    char cCurrentPath[FILENAME_MAX];

    if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath)))
    {
        return -1;
    }

    cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';

    //printf ("The current working directory is %s", cCurrentPath);

    char tpath[256];
    snprintf(tpath, sizeof(tpath), "%s/%s", cCurrentPath, "1-merged-.txt"); tpath[255] = '\0';

    concat c;
    try
    {
        c.setFile(0, tpath);
        c.parsing();
        char str[1024];
        c.read(str, 5, 110);str[110] = '\0';
        debug_print("%s", str);
    } catch(exception &e)
    {
        debug_print("%s", e.what());
    }



    return 0;
}
*/
