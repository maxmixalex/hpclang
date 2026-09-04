volatile unsigned hpclang_rwpi_probe = 11;

int hpclang_start(void)
{
    hpclang_rwpi_probe += 3;
    return (int)hpclang_rwpi_probe;
}
