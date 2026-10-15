#ifndef GATHA_PIC_H
#define GATHA_PIC_H
void pic_init(void);
int pic_is_spurious(unsigned irq);
void pic_eoi(unsigned irq);
#endif
