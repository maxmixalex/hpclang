#include <hpclang/font.h>

static const char *const marked = HPCLANG_UI_TEXT("AB"
                                                  "C");
static const char *const unmarked = "Z";
static const char *const marker_as_data = "HPCLANG_UI_TEXT(\"Q\")";

// HPCLANG_UI_TEXT("X") must not be collected.
/* HPCLANG_UI_TEXT("Y") must not be collected either. */

int main(void)
{
    return marked[0] == 'A' && unmarked[0] == 'Z' && marker_as_data[0] == 'H' ? 0 : 1;
}
