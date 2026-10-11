#ifndef HPCLANG_STDIO_H
#define HPCLANG_STDIO_H
#ifdef __cplusplus
extern "C"
{
#endif
    enum hpclang_stdio_result
    {
        HPCLANG_STDIO_EOF = -1,
        HPCLANG_STDIO_ERROR = -2
    };
    /* Byte callbacks for an explicit process-wide standard-I/O binding. stream is
     * 1 (stdout) or 2 (stderr). write/flush return 0 or -1 with errno. read returns
     * 0..255, EOF, or ERROR with errno (e.g. ECANCELED). read may wait for an event
     * or an edited line; the application owns polling, text mapping and rendering.
     * Missing read returns EOF; missing write/flush succeeds without output.
     * No callback may reenter standard I/O or bind/unbind. Files are unaffected. */
    struct hpclang_stdio_callbacks
    {
        int (*write)(void *context, unsigned stream, unsigned char byte);
        int (*read)(void *context);
        int (*flush)(void *context, unsigned stream);
        void *context;
    };
    /* Copies callbacks, borrows context until unbind; must have read or write.
     * Normal exit flushes live standard streams. Keep the context alive until
     * then, or unbind before leaving its scope. _Exit skips this flushing.
     * One binding at a time: EBUSY rather than silently replacing another owner.
     * Clears standard-stream EOF/error flags and pushback on success. Single-threaded/IRQ-unsafe.
     * Default streams discard output and return EOF. Return 0 or -1/errno. */
    int hpclang_stdio_bind(const struct hpclang_stdio_callbacks *callbacks);
    /* Flush both outputs, always detach, then clear EOF/errors. Returns first flush
     * failure, or 0; idempotent when unbound. Reentrant unbind fails with EBUSY and
     * leaves the binding intact. Unbind before freeing context or a bound console. */
    int hpclang_stdio_unbind(void);
#ifdef __cplusplus
}
#endif
#endif
