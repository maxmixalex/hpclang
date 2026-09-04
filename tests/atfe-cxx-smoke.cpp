#include <string>

volatile int hpclang_cxx_ctor_probe;

struct Probe {
    Probe() { hpclang_cxx_ctor_probe = 42; }
};

static Probe probe;

int main()
{
    std::string value = "hp";
    value += "clang";

    return hpclang_cxx_ctor_probe == 42 && value == "hpclang" ? 0 : 1;
}
