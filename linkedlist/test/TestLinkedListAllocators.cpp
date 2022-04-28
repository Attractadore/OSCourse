#include "LinkedList.h"

#include <gtest/gtest.h>

template<size_t N> void* mallocN(size_t sz) {
    static size_t n = N;
    return (n--) ? std::malloc(sz): nullptr;
}

TEST(LinkedListTest, InitFail) {
    LinkedListAllocator alloc = {
        .alloc = mallocN<0>,
        .free = std::free,
    };
    auto ll = llCreateWithAllocator(&alloc);
    ASSERT_EQ(ll, nullptr);
}

class LinkedListTestAllocators: public testing::Test {
protected:
    LinkedList* ll;
    static constexpr auto c_num_items = 2;

    void SetUp() override {
        LinkedListAllocator alloc = {
            .alloc = mallocN<1 + c_num_items>,
            .free = std::free,
        };
        ll = llCreateWithAllocator(&alloc);
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

TEST_F(LinkedListTestAllocators, InsertFailed) {
    ll_data_t data = -1;   
    auto r = llInsert(ll, llIterPrev(llEnd(ll)), data);
    ASSERT_FALSE(r);
}

TEST_F(LinkedListTestAllocators, AppendFailed) {
    ll_data_t data = -1;   
    auto r = llAppend(ll, data);
    ASSERT_FALSE(r);
}

TEST_F(LinkedListTestAllocators, PrependFailed) {
    ll_data_t data = -1;   
    auto r = llPrepend(ll, data);
    ASSERT_FALSE(r);
}
