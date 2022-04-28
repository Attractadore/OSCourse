#include "LinkedList.h"

#include <gtest/gtest.h>

class LinkedListTestEmpty: public testing::Test {
protected:
    LinkedList* ll;

    void SetUp() override {
        ll = llCreate();
        if (!ll) {
            throw std::bad_alloc{};
        }
    }

    void TearDown() override {
        llDestroy(ll);
    }
};

template<size_t N>
class LinkedListTestWithNItems: public testing::Test {
protected:
    LinkedList* ll;
    static constexpr auto c_num_items = N;

    void SetUp() override {
        ll = llCreate();
        if (!ll) {
            throw std::bad_alloc{};
        }
        for (ll_data_t i = 0; i < c_num_items; i++) {
            if (!llAppend(ll, i)) {
                throw std::bad_alloc{};
            }
        }
    }

    void TearDown() override {
        llDestroy(ll);
    }
};

using LinkedListTestWithSingleItem = LinkedListTestWithNItems<1>;
using LinkedListTestWithItems = LinkedListTestWithNItems<3>;

TEST_F(LinkedListTestEmpty, Init) {
    ASSERT_TRUE(llIsEmpty(ll));
    ASSERT_TRUE(llBegin(ll) == llEnd(ll));
}

TEST_F(LinkedListTestEmpty, IsEmptyEmpty) {
    ASSERT_TRUE(llIsEmpty(ll));
}

TEST_F(LinkedListTestWithItems, IsEmptyNotEmpty) {
    ASSERT_FALSE(llIsEmpty(ll));
}

TEST_F(LinkedListTestWithItems, SizeNotEmpty) {
    for (size_t s = c_num_items; s > 0; s--) {
        ASSERT_EQ(llSize(ll), s);
        llPopBack(ll);
    }
    ASSERT_EQ(llSize(ll), 0);
}

TEST_F(LinkedListTestEmpty, SizeEmpty) {
    ASSERT_EQ(llSize(ll), 0);
}

TEST_F(LinkedListTestWithItems, Insert) {
    auto first = *llFront(ll);
    auto last = *llBack(ll);

    auto prev = llIterNext(llBegin(ll));
    auto next = llIterNext(prev);
    assert(next != llEnd(ll));
    ll_data_t data = -1;
    auto node = llInsert(ll, next, data);
    assert(node);

    ASSERT_EQ(*llIterDeref(node), data);

    ASSERT_EQ(first, *llFront(ll));
    ASSERT_EQ(last, *llBack(ll));

    ASSERT_EQ(llIterNext(prev), node);
    ASSERT_EQ(llIterPrev(node), prev);
    ASSERT_EQ(llIterNext(node), next);
    ASSERT_EQ(llIterPrev(next), node);
}

TEST_F(LinkedListTestWithItems, Append) {
    auto first_data = *llFront(ll);
    auto old_last_it = llIterPrev(llEnd(ll));

    ll_data_t new_data = -1;
    auto r = llAppend(ll, new_data);
    assert(r);

    auto new_last_it = llIterPrev(llEnd(ll));

    ASSERT_EQ(first_data, *llFront(ll));
    ASSERT_EQ(new_data, *llBack(ll));

    ASSERT_EQ(llIterNext(old_last_it), new_last_it);
    ASSERT_EQ(llIterPrev(new_last_it), old_last_it);
}

TEST_F(LinkedListTestEmpty, AppendEmpty) {
    ll_data_t new_data = -1;
    auto r = llAppend(ll, new_data);
    assert(r);

    ASSERT_EQ(new_data, *llFront(ll));
    ASSERT_EQ(new_data, *llBack(ll));

    auto first = llBegin(ll);
    ASSERT_EQ(llIterNext(first), llEnd(ll));
}

TEST_F(LinkedListTestWithItems, Prepend) {
    auto last_data = *llBack(ll);
    auto old_first_it = llBegin(ll);

    ll_data_t new_data = -1;
    auto r = llPrepend(ll, new_data);
    assert(r);

    auto new_first_it = llBegin(ll);

    ASSERT_EQ(new_data, *llFront(ll));
    ASSERT_EQ(last_data, *llBack(ll));

    ASSERT_EQ(llIterPrev(old_first_it), new_first_it);
    ASSERT_EQ(llIterNext(new_first_it), old_first_it);
}

TEST_F(LinkedListTestEmpty, PrependEmpty) {
    ll_data_t new_data = -1;
    auto r = llPrepend(ll, new_data);
    assert(r);

    ASSERT_EQ(new_data, *llFront(ll));
    ASSERT_EQ(new_data, *llBack(ll));

    auto last = llIterPrev(llEnd(ll));
    ASSERT_EQ(llBegin(ll), last);
}

TEST_F(LinkedListTestWithItems, Erase) {
    auto first = llBegin(ll);
    auto last = llIterPrev(llEnd(ll));

    auto prev = llBegin(ll);
    auto node = llIterNext(prev);
    auto next = llIterNext(node);

    llErase(ll, node);

    ASSERT_EQ(llIterNext(prev), next);
    ASSERT_EQ(prev, llIterPrev(next));

    ASSERT_EQ(first, llBegin(ll));
    ASSERT_EQ(last, llIterPrev(llEnd(ll)));
}

TEST_F(LinkedListTestWithItems, PopFront) {
    ll_data_t front = 0;
    do {
        ASSERT_EQ(front, *llFront(ll));
        llPopFront(ll);
        front++;
    } while(not llIsEmpty(ll));
}

TEST_F(LinkedListTestWithItems, PopBack) {
    ll_data_t back = c_num_items - 1;
    do {
        ASSERT_EQ(back, *llBack(ll));
        llPopBack(ll);
        back--;
    } while(not llIsEmpty(ll));
}

TEST_F(LinkedListTestWithItems, Clear) {
    llClear(ll);
    ASSERT_TRUE(llIsEmpty(ll));
}
