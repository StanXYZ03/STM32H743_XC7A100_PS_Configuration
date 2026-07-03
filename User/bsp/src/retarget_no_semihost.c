#include <stdio.h>

/*
 * Keep this file as a harmless stub for the current project.
 *
 * The project already has its own UART-style character redirection in
 * bsp_uart_fifo.c. For the Ethernet-integrated build we avoid forcing ARMCC
 * no-semihosting here, because full stdio still pulls in _sys_open().
 */
#if defined(__CC_ARM)
int ferror(FILE *f)
{
  (void)f;
  return 0;
}

void _ttywrch(int ch)
{
  (void)ch;
}

void _sys_exit(int return_code)
{
  (void)return_code;
  while (1)
  {
  }
}
#endif
