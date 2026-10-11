/* ATFE's picolibc disables __STDIO_EXIT_FLUSH. Keep stream lifetime through
 * public FILE APIs; do not depend on picolibc's private FILE layout. fopen and
 * tmpfile reach fdopen, while the other constructors are wrapped explicitly. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

struct stream_entry
{
    FILE *stream;
    struct stream_entry *next;
    int visited;
};
static struct stream_entry *streams;
static int flushing;
static unsigned closed_standard;

FILE *__real_fdopen(int fd, const char *mode);
FILE *__real_fmemopen(void *buffer, size_t size, const char *mode);
FILE *__real_freopen(const char *path, const char *mode, FILE *stream);
int __real_fclose(FILE *stream);
int __real_fflush(FILE *stream);

static unsigned standard_bit(FILE *stream)
{
    return (stream == stdin ? 1u : 0u) | (stream == stdout ? 2u : 0u) |
           (stream == stderr ? 4u : 0u);
}

static struct stream_entry *reserve(void)
{
    struct stream_entry *entry = malloc(sizeof *entry);
    if (!entry)
        errno = ENOMEM;
    return entry;
}

static FILE *track(struct stream_entry *entry, FILE *stream)
{
    if (!stream) {
        free(entry);
        return NULL;
    }
    *entry = (struct stream_entry){stream, streams, 0};
    streams = entry;
    return stream;
}

static struct stream_entry *find(FILE *stream)
{
    for (struct stream_entry *entry = streams; entry; entry = entry->next)
        if (entry->stream == stream)
            return entry;
    return NULL;
}

static void forget(FILE *stream)
{
    for (struct stream_entry **link = &streams; *link; link = &(*link)->next) {
        if ((*link)->stream == stream) {
            struct stream_entry *entry = *link;
            *link = entry->next;
            free(entry);
            return;
        }
    }
}

FILE *__wrap_fdopen(int fd, const char *mode)
{
    /* Reserve before fdopen: failed registration must not close the caller's
     * descriptor or turn an otherwise successful open into a hidden leak. */
    struct stream_entry *entry = reserve();
    return entry ? track(entry, __real_fdopen(fd, mode)) : NULL;
}

FILE *__wrap_fmemopen(void *buffer, size_t size, const char *mode)
{
    struct stream_entry *entry = reserve();
    return entry ? track(entry, __real_fmemopen(buffer, size, mode)) : NULL;
}

FILE *__wrap_freopen(const char *path, const char *mode, FILE *stream)
{
    struct stream_entry *entry = find(stream);
    struct stream_entry *fresh = entry ? NULL : reserve();
    if (!entry && !fresh) {
        closed_standard |= standard_bit(stream);
        (void)__real_fclose(stream);
        errno = ENOMEM;
        return NULL;
    }
    FILE *result = __real_freopen(path, mode, stream);
    if (!result) {
        int error = errno;
        closed_standard |= standard_bit(stream);
        forget(stream);
        /* The supported libc retains the FILE allocation on reopen failure.
         * Release it as well as the old descriptor before dropping ownership. */
        (void)__real_fclose(stream);
        free(fresh);
        errno = error;
        return NULL;
    }
    closed_standard &= ~standard_bit(result);
    if (entry) {
        entry->stream = result;
        return result;
    }
    return track(fresh, result);
}

int __wrap_fclose(FILE *stream)
{
    closed_standard |= standard_bit(stream);
    forget(stream);
    return __real_fclose(stream);
}

static int flush_standard(void)
{
    FILE *const standard[] = {stdin, stdout, stderr};
    int error = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!(closed_standard & (1u << i)) && !find(standard[i]) &&
            __real_fflush(standard[i]) == EOF && !error)
            error = errno ? errno : EIO;
    }
    return error;
}

int __wrap_fflush(FILE *stream)
{
    if (stream)
        return __real_fflush(stream);
    if (flushing) {
        errno = EBUSY;
        return EOF;
    }
    flushing = 1;
    for (struct stream_entry *entry = streams; entry; entry = entry->next)
        entry->visited = 0;
    int error = flush_standard();
    for (;;) {
        struct stream_entry *entry = streams;
        while (entry && entry->visited)
            entry = entry->next;
        if (!entry)
            break;
        entry->visited = 1;
        /* A custom flush callback may close another stream. Start each search
         * from the live list rather than retaining a possibly freed next link. */
        if (__real_fflush(entry->stream) == EOF && !error)
            error = errno ? errno : EIO;
    }
    flushing = 0;
    if (error)
        errno = error;
    return error ? EOF : 0;
}

void hpclang_stdio_exit_cleanup(void)
{
    (void)flush_standard();
    while (streams)
        (void)__wrap_fclose(streams->stream);
}

#ifdef __PICOLIBC__
FILE *__real_fdevopen(int (*put)(char, FILE *), int (*get)(FILE *), int (*flush)(FILE *));
FILE *__wrap_fdevopen(int (*put)(char, FILE *), int (*get)(FILE *), int (*flush)(FILE *))
{
    struct stream_entry *entry = reserve();
    return entry ? track(entry, __real_fdevopen(put, get, flush)) : NULL;
}

FILE *__real_funopen(const void *cookie, ssize_t (*readfn)(void *, void *, size_t),
                     ssize_t (*writefn)(void *, const void *, size_t),
                     off_t (*seekfn)(void *, off_t, int), int (*closefn)(void *));
FILE *__wrap_funopen(const void *cookie, ssize_t (*readfn)(void *, void *, size_t),
                     ssize_t (*writefn)(void *, const void *, size_t),
                     off_t (*seekfn)(void *, off_t, int), int (*closefn)(void *))
{
    struct stream_entry *entry = reserve();
    return entry ? track(entry, __real_funopen(cookie, readfn, writefn, seekfn, closefn)) : NULL;
}
#endif
