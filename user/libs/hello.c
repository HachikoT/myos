#include "user/libs/stdio.h"
#include "user/libs/ulib.h"

int main(void)
{
    cprintf("Hello world!!.\n");
    cprintf("I am process %d.\n", getpid());
    cprintf("hello pass.\n");
    return 0;
}
