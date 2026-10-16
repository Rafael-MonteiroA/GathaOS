#ifndef GATHA_ATA_H
#define GATHA_ATA_H
#include <stdint.h>
int ata_init(void);
uint32_t ata_sectors(void);
int ata_read(uint32_t sector, void *data);
int ata_write(uint32_t sector, const void *data);
int ata_flush(void);
#endif
