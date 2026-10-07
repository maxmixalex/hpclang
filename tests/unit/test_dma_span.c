#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include "dma-span.h"

static void rejected(const uint32_t *pages, uint32_t count, uint32_t offset, uint32_t bytes,
                     uint32_t alignment, int error)
{
    uint32_t physical = UINT32_C(0x12345678);
    assert(hpclang_dma_span(pages, count, offset, bytes, alignment, &physical) == -1);
    assert(errno == error);
    assert(physical == UINT32_C(0x12345678));
}

int main(void)
{
    uint32_t pages[] = {0x08000ffe, 0x08001ffe, 0x08002ff2, 0x08007ffe};
    uint32_t physical;
    assert(hpclang_dma_span(pages, 4, 32, 100, 32, &physical) == 0);
    assert(physical == 0x08000020);
    /* A gray16 frame can cross pages, including mixed cache attributes. */
    assert(hpclang_dma_span(pages, 4, 4092, 6800, 4, &physical) == 0);
    assert(physical == 0x08000ffc);
    /* An exact endpoint does not require the following page. */
    assert(hpclang_dma_span(pages, 4, 0, 12288, 4096, &physical) == 0);
    rejected(pages, 4, 0, 12289, 1, ENOTSUP);
    assert(hpclang_dma_span(pages, 4, 12288, 4096, 4096, &physical) == 0);
    assert(physical == 0x08007000);

    /* Never accept a hole, duplicate, backwards page or unsupported mapping. */
    const uint32_t bad[] = {0, 0x08000ffe, 0x07fffffe, 0x08001001, 0x08001003, 0x08001002};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        pages[1] = bad[i];
        rejected(pages, 4, 4092, 8, 4, i == 1 || i == 2 ? ENOTSUP : EFAULT);
        /* A bad unrelated page does not invalidate an owned single-page span. */
        assert(hpclang_dma_span(pages, 4, 0, 4096, 4, &physical) == 0);
    }
    pages[1] = 0x08001ffe;
    rejected(pages, 4, 16384, 1, 1, EFAULT);
    rejected(pages, 4, 16383, 2, 1, EFAULT);
    rejected(pages, 4, UINT32_MAX, 8, 1, EFAULT);
    rejected(pages, 4, 4, UINT32_MAX, 1, EFAULT);
    rejected(pages, 4, 2, 4, 4, ERANGE);
    rejected(pages, 4, 0, 0, 1, EINVAL);
    rejected(pages, 4, 0, 4, 0, EINVAL);
    rejected(pages, 4, 0, 4, 3, EINVAL);
    rejected(pages, 257, 0, 4, 1, EINVAL);
    rejected(pages, 0, 0, 4, 1, EINVAL);
    rejected(0, 4, 0, 4, 1, EINVAL);
    assert(hpclang_dma_span(pages, 4, 0, 4, 1, 0) == -1 && errno == EINVAL);

    /* Reject physical wraparound even if the descriptors appear adjacent. */
    const uint32_t wrap[] = {0xfffffffe, 0x00000ffe};
    assert(hpclang_dma_span(wrap, 2, 4092, 4, 4, &physical) == 0);
    assert(physical == 0xfffffffc);
    rejected(wrap, 2, 4092, 8, 4, ERANGE);
    return 0;
}
