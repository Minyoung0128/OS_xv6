// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

void printspaces(int n)
{
  while (n-- > 0)
  {
    consputc(' ');
  }
}

char *int2str(int value, char *str, int base)
{
  char *ptr = str;
  char *ptr1 = str;
  char tmp_char;
  int tmp_value;

  do
  {
    tmp_value = value;
    value /= base;
    *ptr++ = "0123456789abcdef"[tmp_value - value * base];
  } while (value);

  // Apply negative sign for negative numbers
  if (tmp_value < 0)
  {
    *ptr++ = '-';
  }
  *ptr-- = '\0';

  // Reverse the string
  while (ptr1 < ptr)
  {
    tmp_char = *ptr;
    *ptr-- = *ptr1;
    *ptr1++ = tmp_char;
  }

  return str;
}

void uint2str(unsigned int value, char *buffer, int base)
{
  static char digits[] = "0123456789ABCDEF";
  char *ptr = buffer;
  unsigned int num = value;

  // Edge case: value is 0
  if (value == 0)
  {
    *ptr++ = '0';
    *ptr = '\0';
    return;
  }

  // Convert the number to the appropriate base (e.g., 10 for decimal, 16 for hexadecimal)
  while (num > 0)
  {
    *ptr++ = digits[num % base];
    num /= base;
  }

  *ptr = '\0';

  // Reverse the string, since the conversion process produces digits in reverse order
  char *start = buffer;
  char *end = ptr - 1;
  char tmp;

  while (start < end)
  {
    tmp = *start;
    *start = *end;
    *end = tmp;

    start++;
    end--;
  }
}

void uint64_to_str(unsigned int high, unsigned int low, char *buffer, int base)
{
  static char digits[] = "0123456789ABCDEF";
  char *ptr = buffer;

  // Edge case: both high and low are 0
  if (high == 0 && low == 0)
  {
    *ptr++ = '0';
    *ptr = '\0';
    return;
  }

  // Handle the low 32 bits
  unsigned int num = low;
  while (num > 0)
  {
    *ptr++ = digits[num % base];
    num /= base;
  }

  // Handle the high 32 bits (if they exist)
  num = high;
  while (num > 0)
  {
    *ptr++ = digits[num % base];
    num /= base;
  }

  *ptr = '\0';

  // Reverse the string, since the conversion process produces digits in reverse order
  char *start = buffer;
  char *end = ptr - 1;
  char tmp;

  while (start < end)
  {
    tmp = *start;
    *start = *end;
    *end = tmp;

    start++;
    end--;
  }
}

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;

    int width = 0;
    while (c >= '0' && c <= '9')
    {
      width = width * 10 + (c - '0');
      c = fmt[++i] & 0xff;
    }

    switch (c)
    {
    case 'd':
    {
      char buffer[32];
      int2str(*argp++, buffer, 10);
      int len;
      for (len = 0; buffer[len]; len++)
        ; // 문자열의 길이 계산
      if (width > len)
      {
        printspaces(width - len); // 필요한 만큼 공백을 출력
      }
      for (int j = 0; j < len; j++)
      {
        consputc(buffer[j]);
      }
    }
    break;
    
    case 'u':
    {
      char buffer[64];
      uint2str((unsigned int)*argp++, buffer, 10); // 부호 없는 정수를 문자열로 변환
      int len;
      for (len = 0; buffer[len]; len++)
        ; // 문자열의 길이 계산
      if (width > len)
      {
        printspaces(width - len); // 필요한 만큼 공백을 출력
      }
      for (int j = 0; j < len; j++)
      {
        consputc(buffer[j]);
      }
    }
    break;
    case 'U':
    {
      char buffer[64];
      unsigned int high_val = *argp++;
      unsigned int low_val = *argp++;
      uint64_to_str(high_val, low_val, buffer, 10);
      int len;
      for (len = 0; buffer[len]; len++)
        ; // 문자열의 길이 계산
      if (width > len)
      {
        printspaces(width - len); // 필요한 만큼 공백을 출력
      }
      for (int j = 0; j < len; j++)
      {
        consputc(buffer[j]);
      }
    }
    break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
    {
      if ((s = (char *)*argp++) == 0)
        s = "(null)";
      int len;
      for (len = 0; s[len]; len++)
        ; // 문자열의 길이 계산
      if (width > len)
      {
        printspaces(width - len); // 필요한 만큼 공백을 출력
      }
      for (; *s; s++)
        consputc(*s);
    }
    break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

