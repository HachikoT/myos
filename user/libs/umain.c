#include "user/libs/ulib.h"
#include "user/libs/stdio.h"

int main(void);

void umain(void)
{
    cprintf("user start\n");
    int ret = main();
    exit(ret);
}
