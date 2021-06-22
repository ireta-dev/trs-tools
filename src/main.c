#include "trs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char trs_help_message[] = "\
Usage: trs-tools <command> [<flags>...] <trs> [<images>...]\n\
<Commands>\n\
\tc : Create TRS file from images\n\
\tx : Extract images from TRS file";

enum trs_command {
    TRS_COM_NONE = 0,
    TRS_COM_CREATE = 1,
    TRS_COM_EXTRACT = 2,
};

static int has_arg (const char* short_arg, const char* long_arg, const char* arg) {
    if (short_arg != NULL && strcmp (short_arg, arg) == 0)
        return 1;

    if (long_arg != NULL && strcmp (long_arg, arg) == 0)
        return 1;

    return 0;
}

int main (int argc, char* argv[])
{
    if (argc < 3) {
        puts (trs_help_message);
        return 0;
    }

    int return_code = 0;
    int command = 0;
    const char* trs_path = NULL;
    const char** image_paths = malloc (1);
    int image_count = 0;

    for (int arg_index = 1; arg_index < argc; ++arg_index) {
        const char* arg = argv[arg_index];

        if (command == TRS_COM_NONE) {
            /* parse command */

            if (has_arg ("c", NULL, arg)) {
                command = TRS_COM_CREATE;
            } else if (has_arg ("x", NULL, arg)) {
                command = TRS_COM_EXTRACT;
            } else {
                printf ("unknow command '%s'\n", arg);
                puts (trs_help_message);
                return_code = 1;
                goto cleanup;
            }
        } else if (!trs_path) {
             /* parse trs path */

            trs_path = arg;
            int images_max= (argc - 1) - arg_index;
            if (images_max != 0)
                image_paths = realloc (image_paths, sizeof (char*) * images_max);
        } else {
            /* parse images path */

            image_paths[image_count] = arg;
            image_count += 1;
        }
    }

    switch (command) {
    case TRS_COM_NONE:
        printf ("internal error: null command\n");
        return_code = 1;
        goto cleanup;
        break;

    case TRS_COM_CREATE:
        if (image_count == 0) {
            puts ("no input image have been specified\n");
            puts (trs_help_message);
            return_code = 1;
            goto cleanup;
        }

        return_code = trs_make (trs_path, image_paths, image_count);
        goto cleanup;

    case TRS_COM_EXTRACT:
        trs_extract (trs_path);
        goto cleanup;
    }

 cleanup:
    free (image_paths);
    return return_code;
}
