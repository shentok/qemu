/*
 * qdev property parsing
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_CORE_QDEV_PROP_INTERNAL_H
#define HW_CORE_QDEV_PROP_INTERNAL_H

void qdev_propinfo_get_enum(ObjectProperty *oprop, Object *obj, Visitor *v,
                            Error **errp);
void qdev_propinfo_set_enum(ObjectProperty *oprop, Object *obj, Visitor *v,
                            Error **errp);

void qdev_propinfo_set_default_value_enum(ObjectProperty *op,
                                          const Property *prop);
void qdev_propinfo_set_default_value_int(ObjectProperty *op,
                                         const Property *prop);
void qdev_propinfo_set_default_value_uint(ObjectProperty *op,
                                          const Property *prop);

void qdev_propinfo_get_int32(ObjectProperty *oprop, Object *obj, Visitor *v,
                             Error **errp);
void qdev_propinfo_get_size32(ObjectProperty *oprop, Object *obj, Visitor *v,
                              Error **errp);

#endif
