#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
static struct termios dummy;
int main()
{
    (void)dummy;
    return 0;
}
