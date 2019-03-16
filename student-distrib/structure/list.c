#include "list.h"
#include "../lib/cli.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

void list_init(struct linked_list *list) {
    *list = (struct linked_list){
        .first = {
            .value = NULL,
            .prev = NULL,
            .next = &list->last,
        },
        .last = {
            .value = NULL,
            .prev = &list->first,
            .next = NULL,
        },
    };
}

int32_t list_insert_front(struct linked_list *list, void *value) {
    unsigned long flags;

    if (!value)
        return -EINVAL;

    struct list_node *node = kmalloc(sizeof(*node));
    if (!node)
        return -ENOMEM;

    cli_and_save(flags);

    *node = (struct list_node){
        .value = value,
        .prev = &list->first,
        .next = list->first.next,
    };

    node->prev->next = node;
    node->next->prev = node;

    restore_flags(flags);
    return 0;
}
int32_t list_insert_back(struct linked_list *list, void *value) {
    unsigned long flags;

    if (!value)
        return -EINVAL;

    struct list_node *node = kmalloc(sizeof(*node));
    if (!node)
        return -ENOMEM;

    cli_and_save(flags);

    *node = (struct list_node){
        .value = value,
        .prev = list->last.prev,
        .next = &list->last,
    };

    node->prev->next = node;
    node->next->prev = node;

    restore_flags(flags);
    return 0;
}

void *list_pop_front(struct linked_list *list) {
    unsigned long flags;

    if (list->first.next == &list->last)
        return NULL;

    cli_and_save(flags);

    struct list_node *node = list->first.next;
    node->next->prev = node->prev;
    node->prev->next = node->next;

    void *value = node->value;
    kfree(node);

    restore_flags(flags);

    return value;
}
void *list_pop_back(struct linked_list *list) {
    unsigned long flags;

    if (list->first.next == &list->last)
        return NULL;

    cli_and_save(flags);

    struct list_node *node = list->last.prev;
    node->next->prev = node->prev;
    node->prev->next = node->next;

    void *value = node->value;
    kfree(node);

    restore_flags(flags);

    return value;
}

bool list_isempty(struct linked_list *list) {
    return list->first.next == &list->last;
}
bool list_contains(struct linked_list *list, void *value) {
    struct list_node *node;
    list_for_each(list, node) {
        if (node->value == value)
            return true;
    }

    return false;
}
void list_remove(struct linked_list *list, void *value) {
    unsigned long flags;
    cli_and_save(flags);

    struct list_node *node;
    list_for_each(list, node) {
        if (node->value == value) {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            kfree(node);
        }
    }

    restore_flags(flags);
}
