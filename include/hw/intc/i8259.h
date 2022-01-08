#ifndef HW_I8259_H
#define HW_I8259_H

/* i8259.c */

extern I8259CommonState *isa_pic;

/*
 * i8259_init()
 *
 * Create a i8259 device on an ISA @bus,
 * connect its output to @parent_irq_in,
 * return an (allocated) array of 16 input IRQs.
 */
qemu_irq *i8259_init(ISABus *bus, qemu_irq parent_irq_in);
qemu_irq *kvm_i8259_init(ISABus *bus);
int pic_get_output(I8259CommonState *s);
int pic_read_irq(I8259CommonState *s);

#endif
