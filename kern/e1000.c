#include <inc/error.h>
#include <kern/e1000.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <inc/string.h>

// LAB 6: Your driver code here
volatile uint32_t *e1000;

struct tx_desc tx_descarray[E1000_NTX] __attribute__((aligned(16)));
struct rec_desc rec_descarray[E1000_NREC] __attribute__((aligned(16))); 
char tx_buffer[E1000_NTX][TX_PKT_SIZE];
char rec_buffer[E1000_NREC][REC_PKT_SIZE];

int e1000_transmit(const char *buf, uint32_t len) {
    if (len < 0 || len > TX_PKT_SIZE)
        return -E_INVAL;

    uint32_t tdt = e1000[E1000_TDT >> OFF];
    if (!(tx_descarray[tdt].status & E1000_TXD_STAT_DD))
        return -E_TX_FULL;
    
    memset(tx_buffer[tdt], 0, TX_PKT_SIZE);
    memmove(tx_buffer[tdt], buf, len);
    tx_descarray[tdt].length = len;
    tx_descarray[tdt].status &= ~E1000_TXD_STAT_DD;
    tx_descarray[tdt].cmd |= E1000_TXD_CMD_EOP;
    tx_descarray[tdt].cmd |= E1000_TXD_CMD_RS;

   e1000[E1000_TDT >> OFF] = (tdt + 1) % E1000_NTX;
   return 0;
}

int e1000_receive(char *buf, uint32_t* len) {
    uint32_t i = (e1000[E1000_RDT >> OFF] + 1) % E1000_NREC;
    if (!(rec_descarray[i].status & E1000_RXD_STAT_DD)) {
        *len = 0;
        return -E_REC_EMPTY;
    }
    
    *len = rec_descarray[i].length;
    memmove(buf, rec_buffer[i], *len);
    rec_descarray[i].status &= ~E1000_RXD_STAT_DD;
    e1000[E1000_RDT >> OFF] = i % E1000_NREC;
    return 0;
}

int attach_82540EM(struct pci_func *pcif) {
    pci_func_enable(pcif);
    boot_map_region(kern_pgdir, KSTACKTOP, pcif->reg_size[0], pcif->reg_base[0],
        PTE_PCD | PTE_PWT | PTE_P | PTE_W );
    e1000 = (uint32_t *)KSTACKTOP;
    assert(e1000[E1000_STATUS >> OFF] == 0x80080783);

    /* Memory init */
    memset(tx_descarray, 0, sizeof(struct tx_desc) * E1000_NTX);
    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rec_descarray, 0, sizeof(struct rec_desc) * E1000_NREC);
    memset(rec_buffer, 0, sizeof(rec_buffer));
    int i = 0;

    for (; i < E1000_NTX; i++) {
        tx_descarray[i].addr = PADDR(tx_buffer[i]);
        tx_descarray[i].status |= E1000_TXD_STAT_DD;
    }

    for (i = 0; i < E1000_NREC; i++) {
        rec_descarray[i].addr = PADDR(rec_buffer[i]);
    }

    /* Transmit Initialization */
    e1000[E1000_TDBAL >> OFF] = PADDR(tx_descarray);
    e1000[E1000_TDBAH >> OFF] = 0;
    e1000[E1000_TDLEN >> OFF] = sizeof(struct tx_desc) * E1000_NTX;
    e1000[E1000_TDH >> OFF] = 0;
    e1000[E1000_TDT >> OFF] = 0;

    e1000[E1000_TCTL >> OFF] |= E1000_TCTL_EN;
    e1000[E1000_TCTL >> OFF] |= E1000_TCTL_PSP;
    e1000[E1000_TCTL >> OFF] |= ((0X10 << 4) & E1000_TCTL_CT);
    e1000[E1000_TCTL >> OFF] |= ((0X40 << 12) & E1000_TCTL_COLD);
    e1000[E1000_TIPG >> OFF]  = E1000_IPGR2 | E1000_IPGR1 | E1000_IPGT;

    /* Receive Initialization */
    /* MAC address */
    e1000[E1000_RAL >> OFF] = (0x12 << 24) | (0x00 << 16) | (0x54 << 8) | 0x52;
    e1000[E1000_RAH >> OFF] = (0x56 << 8) | 0x34;
    e1000[E1000_RAH >> OFF] |= E1000_RAH_AV;

    /* MTA */
    e1000[E1000_MTA >> OFF] = 0;

    /* Receive queue initializtion */
    e1000[E1000_RDBAL >> OFF] = PADDR(rec_descarray);
    e1000[E1000_RDBAH >> OFF] = 0;

    /* Descriptor size */
    e1000[E1000_RDLEN >> OFF] = sizeof(struct rec_desc) * E1000_NREC;

    e1000[E1000_RDH >> OFF] = 0;
    e1000[E1000_RDT >> OFF] = E1000_NREC - 1;
    
    /* Program the Receive Control */
    e1000[E1000_RCTL >> OFF] = E1000_RCTL_EN;
    e1000[E1000_RCTL >> OFF] |= E1000_RCTL_SZ_2048;
    e1000[E1000_RCTL >> OFF] |= E1000_RCTL_BAM;
    e1000[E1000_RCTL >> OFF] |= E1000_RCTL_SECRC;

    return 1;
}