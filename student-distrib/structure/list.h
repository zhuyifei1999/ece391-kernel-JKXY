#ifndef _LIST_H
#define _LIST_H

#include "../lib/stdint.h"
#include "../lib/stdbool.h"

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
    node = &(list)->first;              \
    node->value;                        \
    node = node->next                   \
)

#endif
