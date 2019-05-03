#include "list.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

void list_init(struct list *list) {
    *list = (struct list){
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

int32_t list_insert_front(struct list *list, void *value) {
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

int32_t list_insert_back(struct list *list, void *value) {
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

int32_t list_insert_ordered(struct list *list, void *value, int (*compare)(const void *, const void *)) {
    unsigned long flags;

    if (!value)
        return -EINVAL;

    struct list_node *node = kmalloc(sizeof(*node));
    if (!node)
        return -ENOMEM;

    cli_and_save(flags);

    // I know, prev-pointers are ugly, but double pointers work best for singly linked lists
    struct list_node *prev;
    for (
        prev = &list->first;
        prev->next->value && compare(prev->next->value, value) > 0;
        prev = prev->next
    );

    *node = (struct list_node){
        .value = value,
        .prev = prev,
        .next = prev->next,
    };

    node->prev->next = node;
    node->next->prev = node;

    restore_flags(flags);
    return 0;
}

void *list_peek_front(struct list *list) {
    if (list->first.next == &list->last)
        return NULL;

    return list->first.next->value;
}
void *list_peek_back(struct list *list) {
    if (list->first.next == &list->last)
        return NULL;

    return list->last.prev->value;
}

void *list_pop_front(struct list *list) {
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
void *list_pop_back(struct list *list) {
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

void list_destroy(struct list *list) {
    while (!list_isempty(list)) {
        list_pop_back(list);
    }
}
bool list_isempty(struct list *list) {
    return list->first.next == &list->last;
}
bool list_contains(struct list *list, void *value) {
    struct list_node *node;
    list_for_each(list, node) {
        if (node->value == value)
            return true;
    }

    return false;
}

void list_remove(struct list *list, void *value) {
    list_remove_on_cond(list, void *, entry, entry == value);
}
