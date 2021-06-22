#ifndef TRS_TRS_H
#define TRS_TRS_H

#include <stddef.h>
#include <stdint.h>

int trs_make (const char* output_path, const char** image_paths, int image_count);
int trs_extract(const char* path);

#endif // TRS_TRS_H
