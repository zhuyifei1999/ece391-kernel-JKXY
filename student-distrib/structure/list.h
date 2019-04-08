#ifndef _LIST_H
#define _LIST_H

#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../lib/cli.h"

// This doublely linked list uses sentinel nodes for both first node
// and last node
struct list_node {
    void *value;
    struct list_node *prev;
    struct list_node *next;
};
struct list {
    struct list_node first;
    struct list_node last;
};

void list_init(struct list *list);

int32_t list_insert_front(struct list *list, void *value);
int32_t list_insert_back(struct list *list, void *value);

void *list_peek_front(struct list *list);
void *list_peek_back(struct list *list);

void *list_pop_front(struct list *list);
void *list_pop_back(struct list *list);

void list_destroy(struct list *list);
bool list_isempty(struct list *list);
bool list_contains(struct list *list, void *value);
void list_remove(struct list *list, void *value);

#define list_for_each(list, node) for ( \
    node = (list)->first.next;          \
    node->value;                        \
    node = node->next                   \
)

#define list_remove_on_cond_extra(list, typ, name, cond, extra) do { \
    unsigned long __flags;                                           \
    cli_and_save(__flags);                                           \
    struct list_node *__node;                                        \
    struct list_node *__next;                                        \
    for (                                                            \
        __node = (list)->first.next;                                 \
        __node->value;                                               \
        __node = __next                                              \
    ) {                                                              \
        typ name = __node->value;                                    \
        __next = __node->next;                                       \
        if (cond) {                                                  \
            __node->next->prev = __node->prev;                       \
            __node->prev->next = __node->next;                       \
            extra;                                                   \
            kfree(__node);                                           \
        }                                                            \
    }                                                                \
    restore_flags(__flags);                                          \
} while (0)

#define list_remove_on_cond(list, typ, name, cond) \
    list_remove_on_cond_extra(list, typ, name, cond, ({}))

#endif
