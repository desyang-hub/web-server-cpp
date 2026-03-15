#pragma once

#include <iostream>

// ======================= DECLARE_POOL_ALLOC ==============================
#define DECLARE_POOL_ALLOC()                        \
public:                                             \
    static void* operator new(size_t size) {        \
        return alloc.allocate(size);                \
    }                                               \
static void operator delete(void* p, size_t size) { \
    return alloc.deallocate(p, size);               \
}                                                   \
protected:                                          \
    static Allocator alloc;                         \
// ======================= DECLARE_POOL_ALLOC ==============================


// ======================= IMPLEMENT_POOL_ALLOC ==============================
#define IMPLEMENT_POOL_ALLOC(CLASS_NAME)\
Allocator CLASS_NAME::alloc \
// ======================= IMPLEMENT_POOL_ALLOC ==============================

class Allocator
{
private:
    struct obj {
        struct obj* next;
    };

    obj* freeStore{nullptr};
    size_t CHUNK{20};
public:
    void* allocate(size_t size) {
        obj* p{nullptr};
        // 申请空间的过程
        if (!freeStore) {
            // 没有空间了,申请空间
            freeStore = (obj*)malloc(size * CHUNK);
            p = freeStore;

            // 使用指针串起来
            for (int i = 1; i < CHUNK; ++i) {
                p->next = (obj*)((char*)p + size);
                p = p->next;
            }
        }

        p = freeStore;
        freeStore = freeStore->next;

        return p;
    }

    void deallocate(void* p, size_t size) {
        static_cast<struct obj*>(p)->next = freeStore;
        freeStore = static_cast<struct obj*>(p);
    }
};
