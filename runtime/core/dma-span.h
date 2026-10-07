#ifndef HPCLANG_DMA_SPAN_H
#define HPCLANG_DMA_SPAN_H

#include <stdint.h>

/*
 * Translate an already-owned buffer in an HPC1 coarse table. The caller must
 * keep both the allocation and its mappings alive until DMA has stopped.
 * This validates physical contiguity; it neither allocates memory nor cleans
 * cached writes. A mapped page alone does not establish buffer ownership.
 *
 * offset is relative to the first page in pages. alignment must be a nonzero
 * power of two. Only HPC1 full-access small-page descriptors are accepted.
 * On failure, physical is unchanged and errno describes the failure.
 */
int hpclang_dma_span(const volatile uint32_t *pages, uint32_t page_count, uint32_t offset,
                     uint32_t bytes, uint32_t alignment, uint32_t *physical);

#endif
