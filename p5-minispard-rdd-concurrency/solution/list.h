// classic linked list implementation
#ifndef __list_h__
#define __list_h__
#include <stdbool.h> 

typedef struct ListNode {
    void* data;            
    struct ListNode* next;
} ListNode;

typedef struct List {
    ListNode* head;
    ListNode* tail;
    int size;
} List;

typedef struct ListIterator {
    ListNode *curr_node; // points to next node to be processed
} ListIterator;

// method headers
List* list_init();
int list_add_elem(List *l, void* e);
int list_set(List* l, int idx, void *newdata);
void* list_remove_elem(List *l);
void* list_get(List *l, int idx);
int list_get_size(List* l);
void list_free(List* l);
ListIterator* list_iterator_begin(List* l);
int list_iterator_has_next(ListIterator* iter); // returns 1 to show that there is next
void* list_iterator_next(ListIterator* iter); // returns pointer of curr head, advances iterator
void list_iterator_destroy(ListIterator* iter); // frees memory with only the iterator, not the acc list

#endif // __list_h__