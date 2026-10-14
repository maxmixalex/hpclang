#ifndef HPCLANG_STAGE_IMAGE_H
#define HPCLANG_STAGE_IMAGE_H

#include <hpclang/abi.h>
#include <stddef.h>

/* Launcher-private HPCZ uses the HPC1 fields followed by raw DEFLATE bytes.
 * The public .hpc format and the application's virtual layout stay HPC1. */
int hpclang_stage_image_valid(const struct hpclang_image_header *image, size_t stored_size);
int hpclang_stage_image_unpack(const struct hpclang_image_header *image, size_t stored_size,
                               void *destination, size_t capacity);

#endif
