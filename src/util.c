#include "util.h"
#include <stdio.h>
#include <stdlib.h>

char *file_to_str(const char *file_path)
{
    FILE *f = fopen(file_path, "rb");
    int fsize;
    char *buffer;

    if (f == NULL)
        return NULL;  

    fseek(f, 0L, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    buffer = malloc(fsize);

    fread(buffer, fsize, 1, f);
    fclose(f);

    return buffer;
}