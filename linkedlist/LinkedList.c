#include "LinkedList.h"

#include <assert.h>
#include <iso646.h>
#include <stdlib.h>

typedef struct LinkedListNodeHeader {
    struct LinkedListNodeHeader* next, *prev;
} LinkedListNodeHeader;

typedef struct LinkedListNode {
    LinkedListNodeHeader header;
    ll_data_t data;
} LinkedListNode;

struct LinkedList {
    LinkedListNodeHeader sent;
    LinkedListAllocator allocator;
};

#define llGetNodeFromIter(it) (_Generic(\
    it,\
    LinkedListIter: (LinkedListNode*) (it)\
))

#define llGetIterFromNode(node) (_Generic(\
    node,\
    LinkedListNodeHeader*: (LinkedListIter) (node),\
    LinkedListNode*      : (LinkedListIter) (node)\
))

static LinkedListNode* llCreateNode(ll_data_t data, const LinkedListAllocator* allocator) {
    LinkedListNode* node = allocator->alloc(sizeof(*node));
    if (!node) {
        return NULL;
    };
    *node = (LinkedListNode) {
        .data = data,
    };
    return node;
}

static void llDestroyNode(LinkedListNode* node, const LinkedListAllocator* allocator) {
    allocator->free(node);
}

static void llInit(LinkedList* list) {
    assert(list);
    list->sent.next = list->sent.prev = &list->sent;
}

LinkedList* llCreate() {
    return llCreateWithAllocator(NULL);
}

LinkedList* llCreateWithAllocator(const LinkedListAllocator* allocator) {
    static LinkedListAllocator ll_default_allocator = {
        .alloc = malloc,
        .free = free
    };
    allocator = allocator ? allocator: &ll_default_allocator;
    LinkedList* list = allocator->alloc(sizeof(*list));
    if (!list) {
        return NULL;
    }
    llInit(list);
    list->allocator = *allocator;
    return list;
}

static void llDestroyNodes(LinkedList* list) {
    assert(list);
    for (
        LinkedListIter it = llBegin(list), end = llEnd(list);
        it != end;
    ) {
        LinkedListIter next = llIterNext(it);
        llDestroyNode(llGetNodeFromIter(it), &list->allocator);
        it = next;
    }
}

void llDestroy(LinkedList* list) {
    if (list) {
        llDestroyNodes(list);
        list->allocator.free(list);
    }
}

ll_data_t* llFront(LinkedList* list) {
    assert(list and !llIsEmpty(list));
    return llIterDeref(llBegin(list));
}

ll_data_t* llBack(LinkedList* list) {
    assert(list and !llIsEmpty(list));
    return llIterDeref(llIterPrev(llEnd(list)));
}

LinkedListIter llBegin(LinkedList* list) {
    assert(list);
    return llGetIterFromNode(list->sent.next);
}

LinkedListIter llEnd(LinkedList* list) {
    assert(list);
    return llGetIterFromNode(&list->sent);
}

LinkedListIter llIterNext(LinkedListIter it) {
    assert(it);
    LinkedListNodeHeader* node = &llGetNodeFromIter(it)->header;
    return llGetIterFromNode(node->next);
}

LinkedListIter llIterPrev(LinkedListIter it) {
    assert(it);
    LinkedListNodeHeader* node = &llGetNodeFromIter(it)->header;
    return llGetIterFromNode(node->prev);
}

ll_data_t* llIterDeref(LinkedListIter it) {
    assert(it);
    LinkedListNode* node = llGetNodeFromIter(it);
    return &node->data;
}

bool llIsEmpty(LinkedList* list) {
    assert(list);
    return llBegin(list) == llEnd(list);
}

size_t llSize(LinkedList* list) {
    assert(list);
    size_t sz = 0;
    for (
        LinkedListIter it = llBegin(list), end = llEnd(list);
        it != end;
        it = llIterNext(it)
    ) {
        sz++;
    }
    return sz;
}

void llClear(LinkedList* list) {
    llDestroyNodes(list);
    llInit(list);
}

LinkedListIter llInsert(
    LinkedList* list, LinkedListIter it, ll_data_t data
) {
    assert(list and it);

    LinkedListNodeHeader* node = &llCreateNode(data, &list->allocator)->header;
    if (!node) {
        return NULL;
    }

    LinkedListNodeHeader* next = &llGetNodeFromIter(it)->header;
    LinkedListNodeHeader* prev = next->prev;
    *node = (LinkedListNodeHeader) {
        .next = next,
        .prev = prev
    };
    prev->next = next->prev = node;

    return llGetIterFromNode(node);
}

LinkedListIter llErase(
    LinkedList* list, LinkedListIter it 
) {
    assert(list and (it and it != llEnd(list)));
    LinkedListNode* node = llGetNodeFromIter(it);
    LinkedListNodeHeader* next = node->header.next;
    LinkedListNodeHeader* prev = node->header.prev;
    prev->next = next;
    next->prev = prev;
    llDestroyNode(node, &list->allocator);
    return llGetIterFromNode(next);
}

LinkedListIter llAppend(LinkedList* list, ll_data_t data) {
    assert(list);
    return llInsert(list, llEnd(list), data);
}

void llPopBack(LinkedList* list) {
    assert(list and not llIsEmpty(list));
    llErase(list, llIterPrev(llEnd(list)));
}

LinkedListIter llPrepend(LinkedList* list, ll_data_t data) {
    assert(list);
    return llInsert(list, llBegin(list), data);
}

void llPopFront(LinkedList* list) {
    assert(list and not llIsEmpty(list));
    llErase(list, llBegin(list));
}
