#include <hpclang/storage.hpp>

int main()
{
    hpclang::owned_stream_directory root{hpclang_stream_directory_open("/")};
    hpclang::owned_stream input{std::fopen("README.TXT", "rb")};
    if (!root || !input)
        return 1;
    (void)std::fgetc(input.get());
    int file_result = input.close();
    int directory_result = root.close();
    return file_result || directory_result ? 1 : 0;
}
