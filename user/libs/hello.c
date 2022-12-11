#include "user/libs/stdio.h"
#include "user/libs/ulib.h"

int main(void)
{
    cprintf("Hello world!!.\n");
    cprintf("I am process %d.\n", getpid());
    cprintf("hello pass.\n");
    cprintf("test.\n");
    int n = 0;
    int m = 100;
    cprintf("number %d\n", m / n);
    return 0;
}
