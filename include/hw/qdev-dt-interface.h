#ifndef QDEV_DT_INTERFACE_H
#define QDEV_DT_INTERFACE_H

#include "qom/object.h"
#include "hw/qdev-core.h"
#include "hw/sysbus.h"

#define TYPE_DEVICE_DT_IF "device-dt-interface"
typedef struct DeviceDeviceTreeIfClass DeviceDeviceTreeIfClass;
DECLARE_CLASS_CHECKERS(DeviceDeviceTreeIfClass, DEVICE_DT_IF, TYPE_DEVICE_DT_IF)
#define DEVICE_DT_IF(obj) \
     INTERFACE_CHECK(DeviceDeviceTreeIf, (obj), TYPE_DEVICE_DT_IF)

struct QDevFdtContext {
    const void *fdt;
    GHashTable *device_map;
};
typedef struct QDevFdtContext QDevFdtContext;

typedef void (*DeviceHandleDeviceTreeNode)(DeviceState *dev, int node,
                                           QDevFdtContext *context);

/**
 * DeviceDeviceTreeIfClass:
 *
 * @handle_device_tree_node: Callback function invoked when the #DeviceState is
 * created from a device tree.
 *
 * Interface is designed for providing generic callback that builds device
 * specific AML blob.
 */
struct DeviceDeviceTreeIfClass {
    InterfaceClass parent_class;

    DeviceHandleDeviceTreeNode handle_device_tree_node_pre;
    DeviceHandleDeviceTreeNode handle_device_tree_node_post;
};

/**
 * qdev_handle_device_tree_node_pre: Acquaint a device with its device tree node.
 * @dev: device to be aquainted
 * @fdt: the whole device tree blob
 * @node: associated node of the device in the device tree blob
 */
void qdev_handle_device_tree_node_pre(DeviceState *dev, int node,
                                      QDevFdtContext *context);

/**
 * qdev_handle_device_tree_node_post: Acquaint a device with its device tree node.
 * @dev: device to be aquainted
 * @fdt: the whole device tree blob
 * @node: associated node of the device in the device tree blob
 */
void qdev_handle_device_tree_node_post(DeviceState *dev, int node,
                                       QDevFdtContext *context);

void machine_fdt_populate(SysBusDevice *sbd, const void *fdt);

void fdt_plaform_populate(SysBusDevice *sbd, QDevFdtContext *context, int parent);

int qdev_fdt_get_num_regs(const void *fdt, int node);

uint64_t qdev_fdt_get_reg_addr(const void *fdt, int node, int i);

uint64_t qemu_fdt_get_reg_size(const void *fdt, int node, int i);

int qemu_fdt_get_num_ranges(const void *fdt, int node);

uint64_t qemu_fdt_get_range_size(const void *fdt, int node, int i);

#endif
