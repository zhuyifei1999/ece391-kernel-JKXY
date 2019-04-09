#include "device.h"
#include "../errno.h"
#include "../mm/kmalloc.h"

struct list dev_registry;
LIST_STATIC_INIT(dev_registry);

int32_t register_dev(uint32_t type, uint32_t dev, struct file_operations *file_op) {
    fill_default_file_op(file_op);

    struct dev_registry_entry *entry = kmalloc(sizeof(*entry));
    if (!entry)
        return -ENOMEM;
    *entry = (struct dev_registry_entry){
        .type = type & S_IFMT,
        .dev = dev,
        .file_op = file_op,
    };

    int32_t res = list_insert_back(&dev_registry, entry);
    if (res < 0)
        kfree(entry);
    return res;
}

struct file_operations *get_dev_file_op(uint32_t type, uint32_t dev) {
    struct list_node *node;
    list_for_each(&dev_registry, node) {
        struct dev_registry_entry *entry = node->value;
        if (entry->type == (type & S_IFMT)) {
            // Exact match
            if (entry->dev == dev)
                return entry->file_op;
            // wildcard for all devices in this major
            if (MINOR(entry->dev) == MINORMASK && MAJOR(entry->dev) == MAJOR(dev))
                return entry->file_op;
        }
    }
    return NULL;
}
