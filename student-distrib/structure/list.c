#include "list.h"
#include "../lib.h"
#include "../mm/kmalloc.h"

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

void list_insert_front(struct linked_list *list, void *value) {
    unsigned long flags;

    if (!value)
        return;

    cli_and_save(flags);

    struct list_node *node = kmalloc(sizeof(*node));
    *node = (struct list_node){
        .value = value,
        .prev = &list->first,
        .next = list->first.next,
    };

    node->prev->next = node;
    node->next->prev = node;

    restore_flags(flags);
}
void list_insert_back(struct linked_list *list, void *value) {
    unsigned long flags;

    if (!value)
        return;

    cli_and_save(flags);

    struct list_node *node = kmalloc(sizeof(*node));
    *node = (struct list_node){
        .value = value,
        .prev = list->last.prev,
        .next = &list->last,
    };

    node->prev->next = node;
    node->next->prev = node;

    restore_flags(flags);
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

bool list_contains(struct linked_list *list, void *value) {
    struct list_node *node;
    for_each(list, node) {
        if (node->value == value)
            return true;
    }

    return false;
}
void list_remove(struct linked_list *list, void *value) {
    unsigned long flags;
    cli_and_save(flags);

    struct list_node *node;
    for_each(list, node) {
        if (node->value == value) {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            kfree(node);
        }
    }

    restore_flags(flags);
}
