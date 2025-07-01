#include <stdio.h>
#include <stdlib.h>
#include <list.h>

// file that defines List structure and its functions

// inits empty list structure
List* list_init() {
    List* list = (List*) malloc(sizeof(List));
    if (list == NULL) {
        return NULL;
    }
    // set everything to nothing
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;

    return list;
}

int list_add_elem(List *l, void* e) {
    ListNode* node = (ListNode*) malloc(sizeof(ListNode));
    if (node == NULL) {
        return -1; // failure
    }
    node->data = e;
    node->next = NULL;

    // list empty so add first node, else add to tail
    if (l->head == NULL || l->size == 0) {
        l->head = node;
        l->tail = node;
    } else {
        l->tail->next = node;
        l->tail = node;
    }
    l->size++;
    return 0; // success
}

int list_set(List *l, int idx, void *newdata){
    if(l == NULL){
        return -1;
    }
    if(idx < 0 || idx >= l->size){
        return -1;
    }

    ListNode *curr_node = l->head;
    for(int i = 0; i < idx; i++){
        curr_node = curr_node->next;
    }

    curr_node->data = newdata;
    return 0;
}

void* list_remove_elem(List *l) {
    if (l == NULL || l->head == NULL) {
        return NULL;
    }
    void* curr_head_data = l->head->data;
    ListNode* new_head = l->head->next;
    // ListNode* curr_head = l->head;
    free(l->head);
    l->head = new_head;
    if (l->head == NULL) { // empty list now
        l->tail = NULL;
    }
    l->size--;
    return curr_head_data;
}

// return data at given idx of List
void* list_get(List *l, int idx) {
    if (l == NULL) {
        return NULL;
    }
    if (idx < 0 || idx >= l->size) {
        return NULL; // might need to change to 0 or something later
    }
    ListNode* curr_node = l->head;
    for (int i = 0; i < idx; i++) {
        if (curr_node == NULL) {
            return NULL; // Return NULL as the list is corrupt
        }
        curr_node = curr_node->next;
    }
    if (curr_node == NULL) {
        return NULL; // Return NULL as the list seems corrupt
   }

    return curr_node->data;
}

int list_get_size(List* l) {
    return l->size;
}

// free all ListNode structs and List struct
void list_free(List* l) {
    ListNode* curr_node = l->head;
    ListNode* next_node = NULL;
    while (curr_node != NULL) {
        next_node = curr_node->next;
        free(curr_node);
        curr_node = next_node;
        l->size--;
    }
    free(l);
}

ListIterator* list_iterator_begin(List* l) {
    if (l == NULL) {
        return NULL;
    }
    ListIterator* iter = (ListIterator*)malloc(sizeof(ListIterator));
    if (iter == NULL) {
        return NULL;
    }
    iter->curr_node = l->head;
    return iter;
}

int list_iterator_has_next(ListIterator* iter) {
    if (iter == NULL) {
        return 0; // invalid iterator
    }
    if (iter->curr_node == NULL) {
        return 0; // does NOT have next
    }
    return 1; // has next
}

void* list_iterator_next(ListIterator* iter) {
    if (iter == NULL || iter->curr_node == NULL) {
        return NULL;
    }
    void* data = iter->curr_node->data;
    iter->curr_node = iter->curr_node->next;
    return data;
}

void list_iterator_destroy(ListIterator* iter) {
    free(iter);
}
