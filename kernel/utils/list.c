#include <os/list.h>

// initialize a list head
void list_init(list_node_t *list)
{
    list->next = list;
    list->prev = list;
}

// add a new entry
static void __list_add(list_node_t *new, list_node_t *prev, list_node_t *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

// add a new entry after the specified head
void list_add(list_node_t *new, list_node_t *head)
{
    __list_add(new, head, head->next);
}

// add a new entry before the specified head
void list_add_tail(list_node_t *new, list_node_t *head)
{
    __list_add(new, head->prev, head);
}

// deletes entry from list
static void __list_del(list_node_t * prev, list_node_t * next)
{
    next->prev = prev;
    prev->next = next;
}

void list_del(list_node_t *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = (void *) 0;
    entry->prev = (void *) 0;
}