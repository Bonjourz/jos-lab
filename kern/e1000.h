#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

#define OFF            2
/* Transmit */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_IPGT     10
#define E1000_IPGR1    (8 << 10)
#define E1000_IPGR2    (6 << 20)

#define E1000_STATUS   0x00008
#define VENDERID_82540EM 0x8086
#define DEVICEID_82540EM 0x100E

#define E1000_NTX 64
#define TX_PKT_SIZE 1518

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_EOP    0x00000001 /* End of Packet */
#define E1000_TXD_CMD_RS     (1 << 3) /* Report Status */
#define E1000_TXD_CMD_RPS    (1 << 4) /* Report Packet Sent */

/* Receive */
#define REC_PKT_SIZE   2048
#define E1000_NREC     128
#define E1000_RAL      0x05400  /* Receive Address Low - RW Array */
#define E1000_RAH      0x05404
/* Receive Address */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */

#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */

#define E1000_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_EEPROM_RW_REG_DATA   16   /* Offset to data in EEPROM read/write registers */
#define E1000_EEPROM_RW_ADDR_SHIFT 8    /* Shift to the address bits */
#define E1000_EEPROM_RW_REG_DONE   0x10 /* Offset to READ/WRITE done bit */
#define E1000_EEPROM_RW_REG_START  1    /* First bit for telling part to start operation */

/* Transmit Descriptor */
struct tx_desc {
    uint64_t addr; 
    uint16_t length; 
    uint8_t cso; 
    uint8_t cmd; 
    uint8_t status; 
    uint8_t css; 
    uint16_t special;
} __attribute__((packed));

struct rec_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

int attach_82540EM(struct pci_func *pcif);
int e1000_transmit(const char *buf, uint32_t len);
int e1000_receive(char *buf, uint32_t* len);

#endif	// JOS_KERN_E1000_H
