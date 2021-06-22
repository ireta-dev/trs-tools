#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "trs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TRS_MAGIC "TCSF"

#define TRS_WORD_SIZE 2
#define TRS_LONG_SIZE 4
#define TRS_PIXEL_SIZE 3
#define TRS_HEADER_SIZE 12
#define TRS_IMAGE_HEADER_SIZE 12

static int trs_is_be ()
{
    const union {
        uint32_t integer;
        uint8_t byte[4];
    } byte_order = {0x01020304};
    return byte_order.byte[0] == 1;
}

static int trs_err(const char * restrict format, ...)
{
    va_list vl;
    va_start(vl, format);
    int code = vfprintf(stderr, format, vl);
    va_end(vl);
    return code;
}

static void trs_rgb565_to_rgb888 (uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b)
{
    /* from: stackoverflow.com/questions/2442576 */
    *r = (((rgb565 & 0xF800) >> 11) * 527 + 23) >> 6;
    *g = (((rgb565 & 0x07E0) >> 5) * 259 + 33) >> 6;
    *b = ((rgb565 & 0x001F) * 527 + 23) >> 6;
}

static uint16_t trs_rgb888_to_rgb565 (uint8_t r, uint8_t g, uint8_t b)
{
    r = (r * 249 + 1014) >> 11;
    g = ((g * 253 + 505) >> 10);
    b = ((b * 249 + 1014) >> 11);
    return (r << 11) | (g << 5) | b;
}

static size_t trs_fwrite_be (const void* buffer, size_t size, size_t count, FILE* file)
{
    if (!trs_is_be ()) {
        const uint8_t* buffer_src = buffer;
        uint8_t* buffer_dst = malloc (size * count);
        for (size_t i = 0; i < count; ++i)  {
            for (size_t j = 0; j < size; ++j) {
                buffer_dst[size * i + (size - 1 - j)] = buffer_src[size * i + j];
            }
        }
        size_t result = fwrite (buffer_dst, size, count, file);
        free (buffer_dst);

        return result;
    } else {
        return fwrite (buffer, size, count, file);
    }
}

static int trs_read_byte (FILE* file, uint8_t* result)
{
    size_t read_length = fread (result, sizeof *result, 1, file);

    if (read_length != 1)
        return 0;

    return 1;
}

static int trs_read_word (FILE* file, uint16_t* result)
{
    uint8_t buffer[TRS_WORD_SIZE];
    size_t read_length = fread (buffer, sizeof *buffer, TRS_WORD_SIZE, file);

    if (read_length != TRS_WORD_SIZE)
        return 0;

    *result = (buffer[0] << 8) | buffer[1];
    return 1;
}

static int trs_read_long (FILE* file, uint32_t* result)
{
    uint8_t buffer[TRS_LONG_SIZE];
    size_t read_length = fread (buffer, sizeof *buffer, TRS_LONG_SIZE, file);

    if (read_length != TRS_LONG_SIZE) return 0;

    *result = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    return 1;
}

/* TODO: support loading images with alpha channel */
/* TODO: support converting specifiled shadow color to magenta */
int trs_make (const char* output_path, const char** image_paths, int image_count) {
    int return_code = 0;
    const int header_section_size = TRS_HEADER_SIZE + (TRS_IMAGE_HEADER_SIZE * image_count);

    int images_width[image_count];
    int images_height[image_count];
    size_t images_offset[image_count];

    size_t images_data_word_offset = 0;
    size_t images_data_size = 0;
    uint16_t* images_data = malloc (sizeof (uint16_t));

    uint8_t* image_buffer = NULL;
    for (int i = 0; i < image_count; ++i) {
        images_offset[i] = header_section_size + (images_data_word_offset * 2);
        if (images_offset[i] > UINT32_MAX) {
            trs_err ("images_offset has exceeded the 32 bits integer limit\n");
            goto error;
        }

        int channels;
        image_buffer = stbi_load(image_paths[i], &images_width[i], &images_height[i], &channels, 3);

        if (image_buffer == NULL) {
            trs_err ("failed to load '%s' due to '%s'\n", image_paths[i], stbi_failure_reason());
            goto error;
        } else if (channels != 3) {
            trs_err ("'%s' has %d color channels when it need to have exactly 3\n", image_paths[i], channels);
            goto error;
        } else if (images_height[i] < 0 || images_width[i] < 0) {
            trs_err ("'%s' (w: %d, h: %d) has either zero width or height\n",
                    image_paths[i],
                    images_width[i],
                    images_height[i]);

            goto error;
        } else if (images_height[i] > 256 || images_width[i] > 256) {
            trs_err ("'%s' (w: %d, h: %d) has either width or height that exceeded 256\n",
                    image_paths[i],
                    images_width[i],
                    images_height[i]);
            goto error;
        }

        images_data_size += images_height[i] * images_width[i];
        images_data = realloc (images_data, sizeof (uint16_t) * images_data_size);
        for (int j = 0; j < images_height[i] * images_width[i] * 3; j += 3) {
            uint8_t r = image_buffer[j];
            uint8_t g = image_buffer[j + 1];
            uint8_t b = image_buffer[j + 2];
            images_data[images_data_word_offset] = trs_rgb888_to_rgb565(r, g, b);
            images_data_word_offset += 1;
        }

        stbi_image_free (image_buffer);
        image_buffer = NULL;
    }

    const uint16_t version = 3;
    const uint16_t zero = 0;
    const uint32_t zero_long = 0;
    FILE* output = fopen(output_path, "wb");
    fwrite(TRS_MAGIC, sizeof (uint8_t), TRS_MAGIC_LENGTH, output); /* magic */
    trs_fwrite_be (&image_count, sizeof (uint16_t), 1, output); /* image_count */
    trs_fwrite_be (&version, sizeof (uint16_t), 1, output); /* version */
    trs_fwrite_be (&zero, sizeof (uint16_t), 1, output); /* scanline */
    trs_fwrite_be (&zero, sizeof (uint16_t), 1, output); /* unused */
    for (int i = 0; i < image_count; ++i) { /* image header */
        uint8_t width = images_width[i] == 256 ? 0 : images_width[i];
        uint8_t height = images_height[i] == 256 ? 0 : images_height[i];
        fwrite (&width, sizeof (uint8_t), 1, output); /* width */
        fwrite (&height, sizeof (uint8_t), 1, output); /* height */
        trs_fwrite_be (&zero, sizeof (uint16_t), 1, output); /* unused */
        trs_fwrite_be (&images_offset[i], sizeof (uint32_t), 1, output); /* unpacked offset */
        fwrite (&zero_long, sizeof (uint32_t), 1, output); /* packed offset */
    }
    trs_fwrite_be (images_data, sizeof (uint16_t), images_data_size, output);

    goto cleanup;
 error:
    return_code = 1;
 cleanup:
    if (image_buffer != NULL)
        stbi_image_free (image_buffer);
    free (images_data);
    return return_code;
}

static int trs_has_valid_magic (FILE* file)
{
    uint8_t buffer[TRS_MAGIC_LENGTH];
    size_t read_length = fread(buffer, sizeof *buffer, TRS_MAGIC_LENGTH, file);
    if (read_length != TRS_MAGIC_LENGTH) return 0;

    int result = memcmp(buffer, TRS_MAGIC, TRS_MAGIC_LENGTH);
    return result == 0;
}

static void trs_ensure_minimum_buffer_size (uint8_t** buffer,
                                            size_t* buffer_size,
                                            size_t buffer_cursor,
                                            size_t added_size)
{
    size_t needed_size = buffer_cursor + added_size + 1;
    if (needed_size > *buffer_size) {
        *buffer_size = needed_size;
        *buffer = realloc (*buffer, needed_size);
        memset (&(*buffer)[*buffer_size], 0, needed_size - (*buffer_size));

        *buffer_size = needed_size;
    }
}

int trs_extract (const char* path)
{
    int return_code = 0;

    FILE* input = fopen (path, "rb");

    if (input == NULL) {
        trs_err ("unable to open '%s'\n", path);
        goto error;
    }

    if (!trs_has_valid_magic (input)) {
        trs_err ("'%s' has incorrected macgic number, this is not a trs file\n");
        goto error;
    }

    uint16_t image_count;
    uint16_t scan_line_length;
    uint16_t version;
    uint16_t unused;

    if (!trs_read_word (input, &image_count) ||
        !trs_read_word (input, &version) ||
        !trs_read_word (input, &scan_line_length) ||
        !trs_read_word (input, &unused)) {
        trs_err ("error while reading header in %s\n", path);
        goto error;
    }

    if (version != 2 && version != 3) {
        trs_err ("unsupported trs version %d\n", version);
        goto error;
    }

    if (unused != 0)
        trs_err ("unused field in header is not zero\n");

    for (int i = 0; i < image_count; ++i) {
        int width;
        int height;
        uint8_t width_data;
        uint8_t height_data;
        uint32_t offset_unpacked;
        uint32_t offset_packed;

        /* load image header */
        fseek (input, TRS_HEADER_SIZE + (TRS_IMAGE_HEADER_SIZE * i), SEEK_SET);
        if (!trs_read_byte (input, &width_data) ||
            !trs_read_byte (input, &height_data) ||
            !trs_read_word (input, &unused) ||
            !trs_read_long (input, &offset_unpacked) ||
            !trs_read_long (input, &offset_packed)) {
            trs_err ("error while reading image headers in %s\n", path);
            goto error;
        }

        if (unused != 0)
            trs_err ("unused field in image header #%d is not zero (%d)\n", i, unused);

        if (offset_unpacked == 0 && offset_packed == 0) {
            trs_err ("both packed and unpacked data offset of image #%d is zero");
            goto error;
        }

        width = width_data == 0 ? 256 : width_data;
        height = height_data == 0 ? 256 : height_data;
        uint8_t image_data[width * height * 3];
#ifndef NDEBUG
        memset (image_data, 0, width * height * 3);
#endif

        /* load image data */
        if (offset_unpacked != 0) {
            fseek (input, offset_unpacked, SEEK_SET);
            for (int pixel_index = 0; pixel_index < width * height; ++pixel_index) {
                uint16_t rgb565_pixel;
                if (!trs_read_word (input, &rgb565_pixel))
                    goto error_image_data;

                uint8_t r, g, b;
                trs_rgb565_to_rgb888 (rgb565_pixel, &r, &g, &b);
                image_data[pixel_index * 3] = r;
                image_data[(pixel_index * 3) + 1] = g;
                image_data[(pixel_index * 3) + 2] = b;
            }
        } else {
            fseek (input, offset_packed, SEEK_SET);

            size_t screen_buffer_cursor = 0;
            size_t screen_buffer_size = scan_line_length * height * TRS_PIXEL_SIZE;
            uint8_t* screen_buffer = calloc (scan_line_length * height * TRS_PIXEL_SIZE, sizeof (uint8_t));
            memset (screen_buffer, 0, scan_line_length * height * TRS_PIXEL_SIZE);

            uint16_t chunk_max_index;
            unsigned int chunk_count;

            if (!trs_read_word (input, &chunk_max_index)) {
                free (screen_buffer);
                goto error_image_data;
            }

            chunk_count = chunk_max_index + 1;

            size_t pixels_recorded = 0;
            for (int chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
                uint16_t screen_offset;
                uint16_t pixel_max_index;

                if (!trs_read_word (input, &screen_offset) ||
                    !trs_read_word (input, &pixel_max_index)) {
                    free (screen_buffer);
                    goto error_image_data;
                }

                /* convert offset from bytes to pixels */
                screen_offset /= 2;

                /* adjust screen pointer offset */
                if (screen_offset > scan_line_length) {
                    screen_offset += pixels_recorded;
                    pixels_recorded = 0;
                } else {
                    pixels_recorded += screen_offset;
                }

                /* record black pixels */
                trs_ensure_minimum_buffer_size (&screen_buffer,
                                                &screen_buffer_size,
                                                screen_buffer_cursor,
                                                screen_offset * TRS_PIXEL_SIZE);
                screen_buffer_cursor += screen_offset * TRS_PIXEL_SIZE;

                /* if pixel_max_index is equal to UINT16_MAX it shoud overflow to zero */
                uint16_t pixel_count = pixel_max_index + 1;

                /* record color pixels */
                trs_ensure_minimum_buffer_size (&screen_buffer,
                                                &screen_buffer_size,
                                                screen_buffer_cursor,
                                                pixel_count * TRS_PIXEL_SIZE);
                for (int pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
                    uint16_t pixel;
                    if (!trs_read_word (input, &pixel)) {
                        free (screen_buffer);
                        goto error_image_data;
                    }

                    uint8_t r, g, b;
                    trs_rgb565_to_rgb888 (pixel, &r, &g, &b);

                    screen_buffer[screen_buffer_cursor] = r;
                    screen_buffer[screen_buffer_cursor + 1] = g;
                    screen_buffer[screen_buffer_cursor + 2] = b;
                    screen_buffer_cursor += 3;
                }
                pixels_recorded += pixel_count;
            }

            /* extract image from screen_buffer */
            size_t image_data_cursor = 0;
            for (int row_index = 0; row_index < height; ++row_index) {
                int row_offset = row_index * scan_line_length * 3;
                memcpy (&image_data[image_data_cursor], &screen_buffer[row_offset], width * 3);
                image_data_cursor += width * 3;
            }

            free (screen_buffer);
        }

        /* write image */
        const char* name_format = "%04d.png";
        int name_length = snprintf (NULL, 0, name_format, i);
        char name_buffer[name_length + 1];
        snprintf (name_buffer, sizeof name_buffer, name_format, i);

        stbi_write_png (name_buffer, width, height, 3, image_data, width * 3);
    }

    goto cleanup;
 error_image_data:
    trs_err ("error while reading image data in %s\n", path);
 error:
    return_code = 1;
 cleanup:
    if (input != NULL)
        fclose (input);
    return return_code;
}
