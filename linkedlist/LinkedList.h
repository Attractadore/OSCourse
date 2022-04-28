#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct LinkedList LinkedList;

typedef int ll_data_t;

typedef void* (*ll_alloc_t)(size_t);
typedef void (*ll_free_t)(void*);
typedef struct LinkedListAllocator {
    ll_alloc_t alloc;
    ll_free_t free;
} LinkedListAllocator;

typedef struct LinkedListIterImpl* LinkedListIter;

LinkedList* llCreate();
/**
 * @param allocator: the allocator to use, NULL for standard malloc and free
 */
LinkedList* llCreateWithAllocator(const LinkedListAllocator* allocator);
void llDestroy(LinkedList* list);

ll_data_t* llFront(LinkedList* list);
ll_data_t* llBack(LinkedList* list);

LinkedListIter llBegin(LinkedList* list);
LinkedListIter llEnd(LinkedList* list);
LinkedListIter llIterNext(LinkedListIter it);
LinkedListIter llIterPrev(LinkedListIter it);
ll_data_t* llIterDeref(LinkedListIter it);

bool llIsEmpty(LinkedList* list);
size_t llSize(LinkedList* list);

void llClear(LinkedList* list);
/**
 * @param it: iterator for the value before which the new value will be inserted
 *
 * @return: iterator to the inserted value
 */
LinkedListIter llInsert(LinkedList* list, LinkedListIter it, ll_data_t data);
/**
 * @return: iterator for the value after the one that has been erased
 */
LinkedListIter llErase(LinkedList* list, LinkedListIter it);

LinkedListIter llAppend(LinkedList* list, ll_data_t data);
void llPopBack(LinkedList* list);
LinkedListIter llPrepend(LinkedList* list, ll_data_t data);
void llPopFront(LinkedList* list);

#ifdef __cplusplus
}
#endif
