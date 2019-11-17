#include <stdio.h>
#include <stdbool.h>
#include <cpm.h>

enum
{
    CISTPL_VERS_1 = 0x15,
    CISTPL_FUNCID = 0x21,
    CISTPL_FUNCE = 0x22,

    REGIDE_DATA = 0,
    REGIDE_ERROR = 1,
    REGIDE_FEATURES = 1,
    REGIDE_SECCOUNT = 2,
    REGIDE_SECNUMBER = 3,
    REGIDE_CYLLO = 4,
    REGIDE_CYLHI = 5,
    REGIDE_DEVHEAD = 6,
    REGIDE_STATUS = 7,
    REGIDE_COMMAND = 7,
    REGIDE_CONTROL = 14,

    IDE_STATUS_ERROR = 0x01,
    IDE_STATUS_DATAREQUEST = 0x08,
    IDE_STATUS_READY = 0x40,
    IDE_STATUS_BUSY = 0x80,

    IDE_CMD_IDENTIFY = 0xEC
};

extern uint8_t read_attr_byte(uint16_t offset);
extern uint8_t read_common_byte(uint16_t offset);
extern void write_common_byte(uint16_t offset, uint8_t byte);

uint16_t attrptr = 0;
uint8_t buffer[512];

void print(const char* s)
{
    for (;;)
    {
        uint8_t b = *s++;
        if (!b)
            return;
        cpm_conout(b);
    }
}

void crlf(void)
{
    print("\r\n");
}

void printx(const char* s)
{
    print(s);
    crlf();
}

void printhex4(uint8_t nibble)
{
    nibble &= 0x0f;
    if (nibble < 10)
        nibble += '0';
    else
        nibble += 'a' - 10;
    cpm_conout(nibble);
}

void printhex8(uint8_t b)
{
    printhex4(b >> 4);
    printhex4(b);
}

bool read_attr(void)
{
    uint8_t link;
    uint8_t* outptr;

    buffer[0] = read_attr_byte(attrptr+0);
    link = buffer[1] = read_attr_byte(attrptr+1);
    if ((buffer[0] == 0xff) || (link == 0xff))
        return false;
    attrptr += 2;

    outptr = buffer+2;
    while (link--)
        *outptr++ = read_attr_byte(attrptr++);
    return true;
}

bool find_attr(uint8_t code)
{
    for (;;)
    {
        if (!read_attr())
            return false;

        if (buffer[0] == code)
            return true;
    }
}

void bad_card(void)
{
    printx("No ATA card found");
    cpm_exit();
}

void delay(void)
{
    volatile int i = 0x100;
    while (i)
        i--;
}

void ide_wait(uint8_t mask)
{
    for (;;)
    {
        uint8_t status = read_common_byte(REGIDE_STATUS);
        if ((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR | mask)) == mask)
            return;

        if ((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR)) == IDE_STATUS_ERROR)
        {
            print("IDE error ");
            printhex8(status);
            crlf();
            cpm_exit();
        }
    }  
}

void ide_read_data(void)
{
    uint8_t* outptr = buffer;
    uint16_t count = 512;

    ide_wait(IDE_STATUS_DATAREQUEST);
    while (count--)
        *outptr++ = read_common_byte(REGIDE_DATA);
}

void main(void)
{
    int i;

    /* Find and print the name, if there is one. */

    if (find_attr(CISTPL_VERS_1))
    {
        print("Found: ");
        print((char*)buffer + 4);
        print(" V");
        printhex8(buffer[2]);
        cpm_conout('.');
        printhex8(buffer[3]);
        crlf();
    }

    /* Check this is a disk card. */

    attrptr = 0;
    if (!find_attr(CISTPL_FUNCID))
        bad_card();
    if (buffer[2] != 0x04 /* fixed disk */)
        bad_card();

    /* Now find a FUNCE indicating this is a ATA card. */

    for (;;)
    {
        if (!read_attr())
            bad_card();
        if (buffer[0] == CISTPL_FUNCID)
            bad_card(); /* Multi-function cards don't work */
        if ((buffer[0] == CISTPL_FUNCE) && ((buffer[2] == 0x01) || (buffer[2] == 0x02)))
            break;
    }
    printx("This is an ATA flash card.");

    write_common_byte(REGIDE_DEVHEAD, 0xe0); /* select master */
    write_common_byte(REGIDE_CONTROL, 0x06); /* reset, no interrupts */
    delay();
    write_common_byte(REGIDE_CONTROL, 0x02); /* release reset, no interrupts */
    delay();
    printx("done reset");

    ide_wait(IDE_STATUS_READY);
    write_common_byte(REGIDE_DEVHEAD, 0xe0); /* select master */
    write_common_byte(REGIDE_FEATURES, 0x01); /* 8-bit mode */
    printx("selected 8-bit mode");

    ide_wait(IDE_STATUS_READY);
    write_common_byte(REGIDE_DEVHEAD, 0xe0); /* select master */
    write_common_byte(REGIDE_COMMAND, IDE_CMD_IDENTIFY);
    ide_read_data();

    for (i=0; i<512; i++)
    {
        printhex8(buffer[i]);
        print(" ");
    }
    crlf();
}
