#include <hpclang/storage.hpp>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    hpclang::owned_stream stream{std::tmpfile()};
    assert(stream);
    assert(std::fputs("saved", stream.get()) >= 0);
    assert(std::fflush(stream.get()) == 0);
    std::rewind(stream.get());
    assert(std::fgetc(stream.get()) == 's');
    int fd = fileno(stream.get());
    auto *original = stream.get();
    hpclang::owned_stream moved{std::move(stream)};
    assert(!stream && moved.get() == original);
    stream.reset(std::tmpfile());
    assert(stream);
    int replaced_fd = fileno(stream.get());
    errno = ERANGE;
    stream = std::move(moved);
    assert(!moved && stream.get() == original && errno == ERANGE);
    assert(fcntl(replaced_fd, F_GETFD) == -1 && errno == EBADF);
    stream.reset(stream.get());
    assert(fcntl(fd, F_GETFD) >= 0);
    auto &alias = stream;
    stream = std::move(alias);
    auto *released = stream.release();
    assert(!stream && released == original);
    stream.reset(released);
    assert(stream.close() == 0 && !stream);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    assert(stream.close() == 0 && errno == EBADF);
    {
        hpclang::owned_stream scoped{std::tmpfile()};
        assert(scoped);
        fd = fileno(scoped.get());
        errno = EDOM;
    }
    assert(errno == EDOM);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    stream.reset(std::tmpfile());
    assert(stream);
    assert(::close(fileno(stream.get())) == 0);
    assert(stream.close() == EOF && errno == EBADF && !stream);
    assert(stream.close() == 0 && errno == EBADF);
    // Destructor cleanup preserves the caller's error even if fclose fails.
    {
        hpclang::owned_stream scoped{std::tmpfile()};
        assert(scoped && ::close(fileno(scoped.get())) == 0);
        errno = ERANGE;
    }
    assert(errno == ERANGE);
}
