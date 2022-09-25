#ifndef HW_I82596_H
#define HW_I82596_H

#define I82596_IOPORT_SIZE       0x20

#include "exec/memory.h"
#include "exec/address-spaces.h"

#define PORT_RESET              0x00    /* reset 82596 */
#define PORT_SELFTEST           0x01    /* selftest */
#define PORT_ALTSCP             0x02    /* alternate SCB address */
#define PORT_ALTDUMP            0x03    /* Alternate DUMP address */
#define PORT_CA                 0x10    /* QEMU-internal CA signal */

typedef struct I82596State_st I82596State;

struct I82596State_st {
    qemu_irq irq;
    NICState *nic;
    NICConf conf;
    QEMUTimer *flush_queue_timer;

    hwaddr scp;         /* pointer to SCP */
    uint8_t sysbus;
    uint32_t scb;       /* SCB */
    uint16_t scb_status;
    uint8_t cu_status, rx_status;
    uint16_t lnkst;

    uint32_t cmd_p;     /* addr of current command */
    int ca;
    int ca_active;
    int send_irq;

    /* Hash register (multicast mask array, multiple individual addresses). */
    uint8_t mult[8];
    uint8_t config[14]; /* config bytes from CONFIGURE command */

    uint8_t tx_buffer[0x4000];
};

ssize_t i82596_receive(NetClientState *nc, const uint8_t *buf, size_t size_);
bool i82596_can_receive(NetClientState *nc);
void i82596_set_link_status(NetClientState *nc);
void i82596_common_init(DeviceState *dev, I82596State *s, NetClientInfo *info);
extern const VMStateDescription vmstate_i82596;
#endif
