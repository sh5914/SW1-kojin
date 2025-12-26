#define MAX_FILES 8
#define DEVICE_UART1 0
#define DEVICE_UART2 1


int fd_table[MAX_FILES] = {
  DEVICE_UART1,
  DEVICE_UART1,
  DEVICE_UART1,
  -1,-1,-1,-1,-1
};


extern void outbyte(unsigned char c, int ch);
extern char inbyte(int ch);


int fcntl(int fd, int cmd, int arg) {
  if(fd < 0 || fd >= MAX_FILES) {
    return -1;
  }

  fd_table[fd] = arg;

  return 0;
}


int read(int fd, char *buf, int nbytes)
{
  char c;
  int  i;
  int device;

  if (fd < 0 || fd >= MAX_FILES) return 0;
  device = fd_table[fd];
  if (device < 0) return 0;
  

  for (i = 0; i < nbytes; i++) {
    c = (char)inbyte(device);

    if (c == '\r' || c == '\n'){ /* CR -> CRLF */
      outbyte('\r', device);
      outbyte('\n', device);
      *(buf + i) = '\n';

    /* } else if (c == '\x8'){ */     /* backspace \x8 */
    } else if (c == '\x7f'){      /* backspace \x8 -> \x7f (by terminal config.) */
      if (i > 0){
	outbyte('\x8', device); /* bs  */
	outbyte(' ', device);   /* spc */
	outbyte('\x8', device); /* bs  */
	i--;
      }
      i--;
      continue;

    } else {
      outbyte(c, device);
      *(buf + i) = c;
    }

    if (*(buf + i) == '\n'){
      return (i + 1);
    }
  }
  return (i);
}

int write (int fd, char *buf, int nbytes)
{
  int i, j;
  int device;

  if (fd < 0 || fd >= MAX_FILES) return 0;
  device = fd_table[fd];
  if (device < 0) return 0;
  
  for (i = 0; i < nbytes; i++) {
    if (*(buf + i) == '\n') {
      outbyte ('\r', device);          /* LF -> CRLF */
    }
    outbyte (*(buf + i), device);
    for (j = 0; j < 300; j++);
  }
  return (nbytes);
}
