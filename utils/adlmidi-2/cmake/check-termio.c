#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
static struct termio dummy;
int main()
{
    (void)dummy;
    return 0;
}
