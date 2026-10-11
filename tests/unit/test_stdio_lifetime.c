#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void hpclang_stdio_exit_cleanup(void);
FILE *__wrap_fdopen(int fd, const char *mode);
/* Exercise the ownership entry explicitly. The ARM E2E calls ordinary fopen
 * and proves the linker's fdopen interception against actual picolibc. */
#define fdopen __wrap_fdopen
void *__real_malloc(size_t size);
static int fail_allocation;
void *__wrap_malloc(size_t size)
{
    return fail_allocation ? NULL : __real_malloc(size);
}

int main(void)
{
    char path[] = "/tmp/hpclang-stdio-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    int observer = dup(fd);
    assert(observer >= 0);
    FILE *file = fdopen(fd, "w");
    assert(file);
    char buffer[128], bytes[32] = {0};
    assert(setvbuf(file, buffer, _IOFBF, sizeof buffer) == 0);
    assert(fputs("first", file) >= 0);
    assert(fflush(NULL) == 0);
    assert(pread(observer, bytes, sizeof bytes, 0) == 5);
    assert(memcmp(bytes, "first", 5) == 0);
    assert(fputs("last", file) >= 0);

    char memory[32] = {0};
    FILE *mem = fmemopen(memory, sizeof memory, "w");
    assert(mem && fputs("memory", mem) >= 0);
    hpclang_stdio_exit_cleanup();
    assert(strcmp(memory, "memory") == 0);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    assert(pread(observer, bytes, sizeof bytes, 0) == 9);
    assert(memcmp(bytes, "firstlast", 9) == 0);
    assert(close(observer) == 0);

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    file = fdopen(fd, "w");
    assert(file && freopen(path, "w", file) == file);
    assert(fputs("reopened", file) >= 0);
    hpclang_stdio_exit_cleanup();
    observer = open(path, O_RDONLY);
    assert(observer >= 0 && read(observer, bytes, sizeof bytes) == 8);
    assert(memcmp(bytes, "reopened", 8) == 0);
    assert(close(observer) == 0);

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    file = fdopen(fd, "w");
    assert(file && fclose(file) == 0);
    observer = open(path, O_RDONLY);
    assert(observer >= 0);
    hpclang_stdio_exit_cleanup();
    assert(fcntl(observer, F_GETFD) >= 0);
    assert(close(observer) == 0);

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    file = fdopen(fd, "w");
    assert(file && close(fd) == 0);
    assert(fclose(file) == EOF);
    hpclang_stdio_exit_cleanup();

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    fail_allocation = 1;
    assert(fdopen(fd, "w") == NULL && errno == ENOMEM);
    fail_allocation = 0;
    assert(fcntl(fd, F_GETFD) >= 0);
    assert(fdopen(fd, "?") == NULL);
    assert(fcntl(fd, F_GETFD) >= 0);
    assert(close(fd) == 0);

    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    file = fdopen(fd, "w");
    assert(file && freopen("/no-such-hpclang-directory/file", "w", file) == NULL);
    hpclang_stdio_exit_cleanup();
    assert(unlink(path) == 0);
    return 0;
}
