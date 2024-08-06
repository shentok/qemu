#include "qemu/osdep.h"
#include "hw/qdev-dt-interface.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/sysbus.h"

#include <libfdt.h>

void qdev_handle_device_tree_node_pre(DeviceState *dev, int node,
                                      QDevFdtContext *context)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_DEVICE_DT_IF)) {
        DeviceDeviceTreeIfClass *dt = DEVICE_DT_IF_GET_CLASS(dev);

        if (dt && dt->handle_device_tree_node_pre) {
            dt->handle_device_tree_node_pre(dev, node, context);
        }
    }
}

void qdev_handle_device_tree_node_post(DeviceState *dev, int node,
                                       QDevFdtContext *context)
{
    if (object_dynamic_cast(OBJECT(dev), TYPE_DEVICE_DT_IF)) {
        DeviceDeviceTreeIfClass *dt = DEVICE_DT_IF_GET_CLASS(dev);

        if (dt && dt->handle_device_tree_node_post) {
            dt->handle_device_tree_node_post(dev, node, context);
        }
    }
}

static hwaddr sysbus_fdt_get_range_parent_addr(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    int parent = fdt_parent_offset(fdt, node);
    int address_cells_child = 4 * fdt_address_cells(fdt, node);
    int size_cells_child = 4 * fdt_size_cells(fdt, node);
    int address_cells_parent = 4 * fdt_address_cells(fdt, parent);
    int entry_size = address_cells_parent + address_cells_child + size_cells_child;
    const struct fdt_property *ranges = fdt_get_property(fdt, node, "ranges", NULL);

    if (!ranges) {
        return 0;
    }

    assert(be32_to_cpu(ranges->len) % entry_size == 0);
    assert(entry_size * i < be32_to_cpu(ranges->len));

    switch (address_cells_parent) {
    case 4:
        return ldl_be_p(&ranges->data[entry_size * i + address_cells_child]);

    case 8:
        return ldq_be_p(&ranges->data[entry_size * i + address_cells_child]);
    }

    return 0;
}

static MemoryRegion *sysbus_fdt_get_range_parent(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    return sysbus_mmio_get_region(sbd, 0);
}

static MemoryRegion *sysbus_fdt_get_range_child(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    return sysbus_mmio_get_region(sbd, i);
}

static uint64_t sysbus_fdt_get_reg_addr(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    return qdev_fdt_get_reg_addr(fdt, node, i);
}

static MemoryRegion *sysbus_fdt_get_mmio(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    return sysbus_mmio_get_region(sbd, 0);
}

static MemoryRegion *sysbus_fdt_get_region(SysBusDevice *sbd, const void *fdt, int node, int i)
{
    int num_ranges = qemu_fdt_get_num_ranges(fdt, node);

    return sysbus_mmio_get_region(sbd, MAX(num_ranges, 0) + i);
}

static int qdev_fdt_get_interrupt_domain(const void *fdt, int node)
{
    int parent = node;
    const struct fdt_property *interrupt_parent;
    uint32_t phandle;

    interrupt_parent = fdt_get_property(fdt, node, "interrupt-parent", NULL);

    while (!interrupt_parent && parent) {
        parent = fdt_parent_offset(fdt, parent);
        interrupt_parent = fdt_get_property(fdt, parent, "interrupt-parent", NULL);
    }

    if (!interrupt_parent) {
        return -1;
    }

    phandle = ldl_be_p(interrupt_parent->data);

    return fdt_node_offset_by_phandle(fdt, phandle);
}

static int qdev_fdt_get_interrupt_cells(const void *fdt, int node)
{
    const struct fdt_property *property;

    property = fdt_get_property(fdt, node, "#interrupt-cells", NULL);

    if (!property) {
        return 0;
    }

    if (be32_to_cpu(property->len) != 4) {
        return 0;
    }

    return ldl_be_p(property->data);
}

static void qdev_fdt_wire_interrupts(QDevFdtContext *context)
{
    gpointer key, value;
    GHashTableIter it;
    const void *fdt = context->fdt;

    g_hash_table_iter_init(&it, context->device_map);

    while (g_hash_table_iter_next(&it, &key, &value)) {
        int node = GPOINTER_TO_INT(key);
        DeviceState *dev = value;
        DeviceState *mpicdev;
        SysBusDevice *s = SYS_BUS_DEVICE(dev);
        const struct fdt_property *interrupts;
        int interrupt_domain;
        int interrupts_len;
        int interrupt_cells;
        interrupts = fdt_get_property(fdt, node, "interrupts", NULL);

        if (!dev) {
            continue;
        }

        if (!interrupts) {
            continue;
        }

        interrupt_domain = qdev_fdt_get_interrupt_domain(fdt, node);
        interrupt_cells = qdev_fdt_get_interrupt_cells(fdt, interrupt_domain);
        interrupts_len = be32_to_cpu(interrupts->len) / 4;

        mpicdev = g_hash_table_lookup(context->device_map,
                                      GINT_TO_POINTER(interrupt_domain));

        for (int i = 0; i < interrupts_len; i += interrupt_cells) {
            uint32_t n = ldl_be_p(&interrupts->data[4 * i]);
            sysbus_connect_irq(s, i / interrupt_cells,
                               qdev_get_gpio_in(mpicdev, n));
        }
    }
}

static int qdev_fdt_gpio_get_cells(const void *fdt, int node)
{
    const struct fdt_property *property;

    property = fdt_get_property(fdt, node, "#gpio-cells", NULL);

    if (!property) {
        return 0;
    }

    if (be32_to_cpu(property->len) != 4) {
        return 0;
    }

    return ldl_be_p(property->data);
}

static void qdev_fdt_wire_gpios(QDevFdtContext *context)
{
    gpointer key, value;
    GHashTableIter it;
    const void *fdt = context->fdt;

    g_hash_table_iter_init(&it, context->device_map);

    while (g_hash_table_iter_next(&it, &key, &value)) {
        int node = GPOINTER_TO_INT(key);
        DeviceState *dev = value;
        const struct fdt_property *gpios = fdt_get_property(fdt, node, "gpios", NULL);
        int i = 0;

        if (!dev) {
            continue;
        }

        if (!gpios) {
            continue;
        }

        for (int ofs = 0; 4 * ofs < be32_to_cpu(gpios->len);) {
            int phandle = ldl_be_p(&gpios->data[4 * ofs]);
            int domain = fdt_node_offset_by_phandle(fdt, phandle);
            int cells = qdev_fdt_gpio_get_cells(fdt, domain);
            uint32_t pin = ldl_be_p(&gpios->data[4 * ofs + 4]);
            DeviceState *gpio_dev;

            gpio_dev = g_hash_table_lookup(context->device_map,
                                           GINT_TO_POINTER(domain));
            qdev_connect_gpio_out(gpio_dev, pin, qdev_get_gpio_in(dev, i));

            ofs += cells + 1;
            i++;
        }
    }
}

void machine_fdt_populate(SysBusDevice *sbd, const void *fdt)
{
    QDevFdtContext context = {
        .fdt = fdt,
        .device_map = g_hash_table_new(NULL, NULL)
    };

    fdt_plaform_populate(sbd, &context, 0);

    qdev_fdt_wire_interrupts(&context);
    qdev_fdt_wire_gpios(&context);

    g_hash_table_destroy(context.device_map);
}

void fdt_plaform_populate(SysBusDevice *sbd, QDevFdtContext *context, int parent)
{
    const void *fdt = context->fdt;
    GHashTable *processed = g_hash_table_new(NULL, NULL);
    gpointer key, value;
    GHashTableIter it;
    int child;

    fdt_for_each_subnode(child, fdt, parent) {
        SysBusDevice *s;
        const struct fdt_property *compatible;
        int num_ranges = qemu_fdt_get_num_ranges(fdt, child);

        compatible = fdt_get_property(fdt, child, "compatible", NULL);

        if (!compatible) {
            continue;
        }

        s = SYS_BUS_DEVICE(qdev_try_new(compatible->data));
        if (s) {
            qdev_handle_device_tree_node_pre(DEVICE(s), child, context);
            sysbus_realize_and_unref(s, &error_fatal);

            for (int i = 0; i < num_ranges; i++) {
                MemoryRegion *child_mr = sysbus_fdt_get_range_child(s, fdt, child, i);
                uint64_t size;
                hwaddr addr;
                MemoryRegion *mr;

                if (!child_mr) {
                    continue;
                }

                size = qemu_fdt_get_range_size(fdt, child, i);
                addr = sysbus_fdt_get_range_parent_addr(s, fdt, child, i);
                mr = sysbus_fdt_get_range_parent(sbd, fdt, child, i);

                assert(size);
                memory_region_set_size(child_mr, size);
                memory_region_add_subregion(mr, addr, child_mr);
            }
        } else {
            qemu_log_mask(LOG_UNIMP, "Unimplemented device type %s\n",
                          compatible->data);
        }

        g_hash_table_insert(context->device_map, GINT_TO_POINTER(child),
                            DEVICE(s));
        g_hash_table_insert(processed, GINT_TO_POINTER(child), s);
    }

    g_hash_table_iter_init(&it, processed);

    while (g_hash_table_iter_next(&it, &key, &value)) {
        int node = GPOINTER_TO_INT(key);
        SysBusDevice *s = value;
        int num_regs = qdev_fdt_get_num_regs(fdt, node);

        if (!s) {
            continue;
        }

        if (num_regs < 0) {
            continue;
        }

        for (int i = 0; i < num_regs; i++) {
            uint64_t addr = sysbus_fdt_get_reg_addr(s, fdt, node, i);
            uint64_t size = qemu_fdt_get_reg_size(fdt, node, i);
            MemoryRegion *mr = sysbus_fdt_get_mmio(sbd, fdt, node, i);
            MemoryRegionSection section = memory_region_find(mr, addr, size);
            MemoryRegion *child_mr = sysbus_fdt_get_region(s, fdt, node, i);

            memory_region_set_size(child_mr, size);

            if (section.size) {
                memory_region_add_subregion(section.mr,
                        section.offset_within_region, child_mr);
            } else {
                memory_region_add_subregion(mr, addr, child_mr);
            }
        }

        qdev_handle_device_tree_node_post(DEVICE(s), node, context);
    }

    g_hash_table_destroy(processed);
}

int qdev_fdt_get_num_regs(const void *fdt, int node)
{
    const struct fdt_property *reg = fdt_get_property(fdt, node, "reg", NULL);
    int parent = fdt_parent_offset(fdt, node);
    int address_cells = fdt_address_cells(fdt, parent);
    int size_cells = fdt_size_cells(fdt, parent);
    int entry_size = 4 * (address_cells + size_cells);
    int reg_len;

    if (!reg) {
        return 0;
    }

    reg_len = be32_to_cpu(reg->len);

    if (reg_len % entry_size != 0) {
        return -1;
    }

    return reg_len / entry_size;
}

uint64_t qdev_fdt_get_reg_addr(const void *fdt, int node, int i)
{
    const struct fdt_property *reg = fdt_get_property(fdt, node, "reg", NULL);
    int parent = fdt_parent_offset(fdt, node);
    int address_cells = fdt_address_cells(fdt, parent);
    int size_cells = fdt_size_cells(fdt, parent);
    int entry_size = 4 * (address_cells + size_cells);
    int num_regs = qdev_fdt_get_num_regs(fdt, node);

    if (num_regs < 0) {
        return -1;
    }

    if (i >= num_regs) {
        return -1;
    }

    assert(reg);

    switch (address_cells) {
    case 1:
        return ldl_be_p(&reg->data[entry_size * i]);
    case 2:
        return ldq_be_p(&reg->data[entry_size * i]);
    }

    return -1;
}

uint64_t qemu_fdt_get_reg_size(const void *fdt, int node, int i)
{
    const struct fdt_property *reg = fdt_get_property(fdt, node, "reg", NULL);
    int parent = fdt_parent_offset(fdt, node);
    int address_cells = 4 * fdt_address_cells(fdt, parent);
    int size_cells = 4 * fdt_size_cells(fdt, parent);
    int entry_size = address_cells + size_cells;
    int num_regs = qdev_fdt_get_num_regs(fdt, node);

    if (num_regs < 0) {
        return -1;
    }

    if (i >= num_regs) {
        return -1;
    }

    assert(reg);

    switch (size_cells) {
    case 4:
        return ldl_be_p(&reg->data[entry_size * i + address_cells]);
    case 8:
        return ldq_be_p(&reg->data[entry_size * i + address_cells]);
    }

    return -1;
}

int qemu_fdt_get_num_ranges(const void *fdt, int node)
{
    int num_ranges = -1;
    const struct fdt_property *ranges;

    ranges = fdt_get_property(fdt, node, "ranges", NULL);

    if (ranges) {
        int parent = fdt_parent_offset(fdt, node);
        int ranges_len = be32_to_cpu(ranges->len);
        int entry_size =
            fdt_address_cells(fdt, parent) +
            fdt_address_cells(fdt, node) +
            fdt_size_cells(fdt, node);
        entry_size *= 4;

        if (ranges_len % entry_size == 0) {
            num_ranges = ranges_len / entry_size;
        }
    }

    return num_ranges;
}

uint64_t qemu_fdt_get_range_size(const void *fdt, int node, int i)
{
    int parent = fdt_parent_offset(fdt, node);
    int address_cells_child = 4 * fdt_address_cells(fdt, node);
    int size_cells_child = 4 * fdt_size_cells(fdt, node);
    int address_cells_parent = 4 * fdt_address_cells(fdt, parent);
    int entry_size = address_cells_parent + address_cells_child + size_cells_child;
    const struct fdt_property *ranges = fdt_get_property(fdt, node, "ranges", NULL);

    if (!ranges) {
        return 0;
    }

    assert(be32_to_cpu(ranges->len) % entry_size == 0);
    assert(entry_size * i < be32_to_cpu(ranges->len));

    switch (size_cells_child) {
    case 4:
        return ldl_be_p(&ranges->data[entry_size * i + address_cells_child + address_cells_parent]);

    case 8:
        return ldq_be_p(&ranges->data[entry_size * i + address_cells_child + address_cells_parent]);
    }

    return 0;
}

static const TypeInfo types[] = {
    {
        .name       = TYPE_DEVICE_DT_IF,
        .parent     = TYPE_INTERFACE,
        .class_size = sizeof(DeviceDeviceTreeIfClass),
    },
};

DEFINE_TYPES(types)
