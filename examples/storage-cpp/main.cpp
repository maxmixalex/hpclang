#include <hpclang/storage.hpp>

int main()
{
    hpclang::owned_stream_directory root{hpclang_stream_directory_open("/")};
    hpclang::owned_stream input{std::fopen("README.TXT", "rb")};
    if (!root || !input)
        return 1;
    // An empty input is valid; a failed read is not a successful operation.
    if (std::fgetc(input.get()) == EOF && std::ferror(input.get()))
        return 1;
    // Explicit close reports errors. Destructors provide fallback cleanup on
    // earlier returns, but cannot certify a successful write or close.
    int file_result = input.close();
    int directory_result = root.close();
    return file_result || directory_result ? 1 : 0;
}
