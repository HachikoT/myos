#include "kern/driver/console.h"
#include "libs/defs.h"
#include "libs/x86.h"
#include "libs/string.h"
#include "kern/driver/picirq.h"
#include "kern/driver/kbdreg.h"
#include "kern/driver/stdio.h"
#include "kern/mm/mem_layout.h"

/***** Serial I/O code *****/
#define COM1 0x3F8

#define COM_RX 0           // In:  Receive buffer (DLAB=0)
#define COM_TX 0           // Out: Transmit buffer (DLAB=0)
#define COM_DLL 0          // Out: Divisor Latch Low (DLAB=1)
#define COM_DLM 1          // Out: Divisor Latch High (DLAB=1)
#define COM_IER 1          // Out: Interrupt Enable Register
#define COM_IER_RDI 0x01   // Enable receiver data interrupt
#define COM_IIR 2          // In:  Interrupt ID Register
#define COM_FCR 2          // Out: FIFO Control Register
#define COM_LCR 3          // Out: Line Control Register
#define COM_LCR_DLAB 0x80  // Divisor latch access bit
#define COM_LCR_WLEN8 0x03 // Wordlength: 8 bits
#define COM_MCR 4          // Out: Modem Control Register
#define COM_MCR_RTS 0x02   // RTS complement
#define COM_MCR_DTR 0x01   // DTR complement
#define COM_MCR_OUT2 0x08  // Out2 complement
#define COM_LSR 5          // In:  Line Status Register
#define COM_LSR_DATA 0x01  // Data available
#define COM_LSR_TXRDY 0x20 // Transmit buffer avail
#define COM_LSR_TSRE 0x40  // Transmitter off

#define MONO_BASE 0x3B4
#define MONO_BUF (0xB0000 + KERN_BASE)
#define CGA_BASE 0x3D4
#define CGA_BUF (0xB8000 + KERN_BASE)
#define CRT_ROWS 25
#define CRT_COLS 80
#define CRT_SIZE (CRT_ROWS * CRT_COLS)

#define LPTPORT 0x378

/* stupid I/O delay routine necessitated by historical PC design flaws */
static void delay(void)
{
    inb(0x84);
    inb(0x84);
    inb(0x84);
    inb(0x84);
}

static uint16_t g_cursor_pos; // 当前游标的位置，等于行数*80+列数
static uint16_t *g_crt_buf;   // 显存数组
static uint16_t g_addr_6845;  // 控制端口

// 初始化文本模式显示适配器，可以显示25行80列的ASCII字符，修改显存就可以修改显示的内容
static void cga_init(void)
{
    // 判断彩色文本模式是否可用，否则使用黑白的
    // CGA_BUF 0xB8000，彩色文本模式显存
    // MONO_BUF 0xB8000，黑白文本模式显存
    volatile uint16_t *cp = (uint16_t *)CGA_BUF;
    uint16_t was = *cp;
    *cp = (uint16_t)0xA55A;
    if (*cp != 0xA55A)
    {
        cp = (uint16_t *)MONO_BUF;
        g_addr_6845 = MONO_BASE;
    }
    else
    {
        *cp = was;
        g_addr_6845 = CGA_BASE;
    }
    g_crt_buf = (uint16_t *)cp;

    // 获取当前的游标位置
    uint16_t pos;
    outb(g_addr_6845, 14);
    pos = inb(g_addr_6845 + 1) << 8;
    outb(g_addr_6845, 15);
    pos |= inb(g_addr_6845 + 1);

    g_cursor_pos = pos;
}

// 向显示器写一个字符，显存中每两个字节表示一个字符，结构如下
//
// +-1-+-1-+-1-+-1-+-1-+-1-+-1-+-1-+-----8------+
// | K | R | G | B | I | R | G | B | ASCII编码  |
// +---+---+---+---+---+---+---+---+------------+
//
// K：是否闪烁
// 第一个RGB：背景色
// I：前景色深浅
// 第二个RGB：前景色
//
static void cga_putc(int c)
{
    // 没有设置颜色的默认白字黑底
    if (!(c & ~0xFF))
    {
        c |= 0x0700;
    }

    switch (c & 0xff)
    {
    case '\b':
        if (g_cursor_pos > 0)
        {
            g_cursor_pos--;
            g_crt_buf[g_cursor_pos] = (c & ~0xff) | ' ';
        }
        break;
    case '\n':
        g_cursor_pos += CRT_COLS;
    case '\r':
        g_cursor_pos -= (g_cursor_pos % CRT_COLS);
        break;
    default:
        g_crt_buf[g_cursor_pos++] = c;
        break;
    }

    // 当超过一页，向后滚动一行
    if (g_cursor_pos >= CRT_SIZE)
    {
        memmove(g_crt_buf, g_crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        for (int i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
        {
            // 最后一行内容初始化为空格
            g_crt_buf[i] = 0x0700 | ' ';
        }
        g_cursor_pos -= CRT_COLS;
    }

    // 移动游标到新的位置
    outb(g_addr_6845, 14);
    outb(g_addr_6845 + 1, g_cursor_pos >> 8);
    outb(g_addr_6845, 15);
    outb(g_addr_6845 + 1, g_cursor_pos);
}

static bool serial_exists = 0;

static void serial_init(void)
{
    // Turn off the FIFO
    outb(COM1 + COM_FCR, 0);

    // Set speed; requires DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_DLAB);
    outb(COM1 + COM_DLL, (uint8_t)(115200 / 9600));
    outb(COM1 + COM_DLM, 0);

    // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

    // No modem controls
    outb(COM1 + COM_MCR, 0);
    // Enable rcv interrupts
    outb(COM1 + COM_IER, COM_IER_RDI);

    // Clear any preexisting overrun indications and interrupts
    // Serial port doesn't exist if COM_LSR returns 0xFF
    serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
    (void)inb(COM1 + COM_IIR);
    (void)inb(COM1 + COM_RX);

    if (serial_exists)
    {
        pic_enable(IRQ_COM1);
    }
}

static void lpt_putc_sub(int c)
{
    int i;
    for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i++)
    {
        delay();
    }
    outb(LPTPORT + 0, c);
    outb(LPTPORT + 2, 0x08 | 0x04 | 0x01);
    outb(LPTPORT + 2, 0x08);
}

/* lpt_putc - copy console output to parallel port */
static void lpt_putc(int c)
{
    if (c != '\b')
    {
        lpt_putc_sub(c);
    }
    else
    {
        lpt_putc_sub('\b');
        lpt_putc_sub(' ');
        lpt_putc_sub('\b');
    }
}

static void
serial_putc_sub(int c)
{
    int i;
    for (i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i++)
    {
        delay();
    }
    outb(COM1 + COM_TX, c);
}

/* serial_putc - print character to serial port */
static void
serial_putc(int c)
{
    if (c != '\b')
    {
        serial_putc_sub(c);
    }
    else
    {
        serial_putc_sub('\b');
        serial_putc_sub(' ');
        serial_putc_sub('\b');
    }
}

// 控制台输入缓存区
#define CONSBUFSIZE 512

static struct
{
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos;
    uint32_t wpos;
} cons;

/* *
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 * */
static void cons_intr(int (*proc)(void))
{
    int c;
    while ((c = (*proc)()) != -1)
    {
        if (c != 0)
        {
            cons.buf[cons.wpos++] = c;
            if (cons.wpos == CONSBUFSIZE)
            {
                cons.wpos = 0;
            }
        }
    }
}

/* serial_proc_data - get data from serial port */
static int
serial_proc_data(void)
{
    if (!(inb(COM1 + COM_LSR) & COM_LSR_DATA))
    {
        return -1;
    }
    int c = inb(COM1 + COM_RX);
    if (c == 127)
    {
        c = '\b';
    }
    return c;
}

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void)
{
    if (serial_exists)
    {
        cons_intr(serial_proc_data);
    }
}

/***** Keyboard input code *****/

#define NO 0

#define SHIFT (1 << 0)
#define CTL (1 << 1)
#define ALT (1 << 2)

#define CAPSLOCK (1 << 3)
#define NUMLOCK (1 << 4)
#define SCROLLLOCK (1 << 5)

#define E0ESC (1 << 6)

static uint8_t shiftcode[256] = {
    [0x1D] = CTL,
    [0x2A] = SHIFT,
    [0x36] = SHIFT,
    [0x38] = ALT,
    [0x9D] = CTL,
    [0xB8] = ALT};

static uint8_t togglecode[256] = {
    [0x3A] = CAPSLOCK,
    [0x45] = NUMLOCK,
    [0x46] = SCROLLLOCK};

static uint8_t normalmap[256] = {
    NO, 0x1B, '1', '2', '3', '4', '5', '6', // 0x00
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', // 0x10
    'o', 'p', '[', ']', '\n', NO, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', // 0x20
    '\'', '`', NO, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', NO, '*', // 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7', // 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO, // 0x50
    [0xC7] = KEY_HOME, [0x9C] = '\n' /*KP_Enter*/,
    [0xB5] = '/' /*KP_Div*/, [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP, [0xCB] = KEY_LF,
    [0xCD] = KEY_RT, [0xCF] = KEY_END,
    [0xD0] = KEY_DN, [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS, [0xD3] = KEY_DEL};

static uint8_t shiftmap[256] = {
    NO, 033, '!', '@', '#', '$', '%', '^', // 0x00
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', // 0x10
    'O', 'P', '{', '}', '\n', NO, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', // 0x20
    '"', '~', NO, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', NO, '*', // 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7', // 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO, // 0x50
    [0xC7] = KEY_HOME, [0x9C] = '\n' /*KP_Enter*/,
    [0xB5] = '/' /*KP_Div*/, [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP, [0xCB] = KEY_LF,
    [0xCD] = KEY_RT, [0xCF] = KEY_END,
    [0xD0] = KEY_DN, [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS, [0xD3] = KEY_DEL};

#define C(x) (x - '@')

static uint8_t ctlmap[256] = {
    NO, NO, NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, NO,
    C('Q'), C('W'), C('E'), C('R'), C('T'), C('Y'), C('U'), C('I'),
    C('O'), C('P'), NO, NO, '\r', NO, C('A'), C('S'),
    C('D'), C('F'), C('G'), C('H'), C('J'), C('K'), C('L'), NO,
    NO, NO, NO, C('\\'), C('Z'), C('X'), C('C'), C('V'),
    C('B'), C('N'), C('M'), NO, NO, C('/'), NO, NO,
    [0x97] = KEY_HOME,
    [0xB5] = C('/'), [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP, [0xCB] = KEY_LF,
    [0xCD] = KEY_RT, [0xCF] = KEY_END,
    [0xD0] = KEY_DN, [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS, [0xD3] = KEY_DEL};

static uint8_t *charcode[4] = {
    normalmap,
    shiftmap,
    ctlmap,
    ctlmap};

/* *
 * kbd_proc_data - get data from keyboard
 *
 * The kbd_proc_data() function gets data from the keyboard.
 * If we finish a character, return it, else 0. And return -1 if no data.
 * */
static int kbd_proc_data(void)
{
    int c;
    uint8_t data;
    static uint32_t shift;

    // 判断是否有缓存输入数据
    if ((inb(KBSTATP) & KBS_DIB) == 0)
    {
        return -1;
    }

    data = inb(KBDATAP);

    if (data == 0xE0)
    {
        // E0 escape character
        shift |= E0ESC;
        return 0;
    }
    else if (data & 0x80)
    {
        // Key released
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    }
    else if (shift & E0ESC)
    {
        // Last character was an E0 escape; or with 0x80
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK)
    {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }

    // Process special keys
    // Ctrl-Alt-Del: reboot
    if (!(~shift & (CTL | ALT)) && c == KEY_DEL)
    {
        cprintf("Rebooting!\n");
        outb(0x92, 0x3); // courtesy of Chris Frost
    }
    return c;
}

/* kbd_intr - try to feed input characters from keyboard */
void kbd_intr(void)
{
    cons_intr(kbd_proc_data);
}

static void kbd_init(void)
{
    // drain the kbd buffer
    kbd_intr();
    pic_enable(IRQ_KBD);
}

// 初始化控制台console
void cons_init(void)
{
    // 初始化文本模式显示适配器
    cga_init();

    // 初始化串口COM1
    serial_init();

    // 初始化键盘
    kbd_init();

    if (!serial_exists)
    {
        cprintf("serial port does not exist!!\n");
    }
}

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c)
{
    lpt_putc(c);
    cga_putc(c);
    serial_putc(c);
}

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
int cons_getc(void)
{
    int c;

    // poll for any pending input characters,
    // so that this function works even when interrupts are disabled
    // (e.g., when called from the kernel monitor).
    serial_intr();
    kbd_intr();

    // grab the next character from the input buffer.
    if (cons.rpos != cons.wpos)
    {
        c = cons.buf[cons.rpos++];
        if (cons.rpos == CONSBUFSIZE)
        {
            cons.rpos = 0;
        }
        return c;
    }
    return 0;
}
