
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "heap.h"
#include "redismodule.h"
#include "../consts.h"
#include "../load_io_error_macros.h"

#define DEFAULT_CAPACITY 13

size_t heap_sizeof(unsigned int size) {
    return sizeof(heap_t) + size * sizeof(void *);
}

static int __child_left(const int idx) {
    return idx * 2 + 1;
}

static int __child_right(const int idx) {
    return idx * 2 + 2;
}

static int __parent(const int idx) {
    return (idx - 1) / 2;
}

void heap_init(heap_t *h,
               int (*cmp)(const void *, const void *, const void *udata),
               const void *udata,
               unsigned int size) {
    h->cmp = cmp;
    h->udata = udata;
    h->size = size;
    h->count = 0;
}

heap_t *heap_new(int (*cmp)(const void *, const void *, const void *udata),
                 const void *udata,
                 const size_t *cap) {
    size_t _cap = (!cap) ? DEFAULT_CAPACITY : *cap;
    heap_t *h = malloc(heap_sizeof(_cap));

    if (!h)
        return NULL;

    heap_init(h, cmp, udata, _cap);

    return h;
}

void heap_free(heap_t *h) {
    free(h);
}

/**
 * @return a new heap on success; NULL otherwise */
static heap_t *__ensurecapacity(heap_t *h) {
    if (h->count < h->size)
        return h;

    h->size *= 2;

    return realloc(h, heap_sizeof(h->size));
}

static void __swap(heap_t *h, const int i1, const int i2) {
    void *tmp = h->array[i1];

    h->array[i1] = h->array[i2];
    h->array[i2] = tmp;
}

static int __pushup(heap_t *h, unsigned int idx) {
    /* 0 is the root node */
    while (0 != idx) {
        int parent = __parent(idx);

        /* we are smaller than the parent */
        if (h->cmp(h->array[idx], h->array[parent], h->udata) < 0)
            return -1;
        else
            __swap(h, idx, parent);

        idx = parent;
    }

    return idx;
}

static void __pushdown(heap_t *h, unsigned int idx) {
    while (1) {
        unsigned int childl, childr, child;

        childl = __child_left(idx);
        childr = __child_right(idx);

        if (childr >= h->count) {
            /* can't pushdown any further */
            if (childl >= h->count)
                return;

            child = childl;
        }
        /* find biggest child */
        else if (h->cmp(h->array[childl], h->array[childr], h->udata) < 0)
            child = childr;
        else
            child = childl;

        /* idx is smaller than child */
        if (h->cmp(h->array[idx], h->array[child], h->udata) < 0) {
            __swap(h, idx, child);
            idx = child;
            /* bigger than the biggest child, we stop, we win */
        } else
            return;
    }
}

static void __heap_offerx(heap_t *h, void *item) {
    h->array[h->count] = item;

    /* ensure heap properties */
    __pushup(h, h->count++);
}

int heap_offerx(heap_t *h, void *item) {
    if (h->count == h->size)
        return -1;
    __heap_offerx(h, item);
    return 0;
}

int heap_offer(heap_t **h, void *item) {
    if (NULL == (*h = __ensurecapacity(*h)))
        return -1;

    __heap_offerx(*h, item);
    return 0;
}

void *heap_poll(heap_t *h) {
    if (0 == heap_count(h))
        return NULL;

    void *item = h->array[0];

    h->array[0] = h->array[h->count - 1];
    h->count--;

    if (h->count > 1)
        __pushdown(h, 0);

    return item;
}

static void __heap_replacex(heap_t *h, void *item) {
    h->array[0] = item;

    /* ensure heap properties */
    __pushdown(h, 0);
}

void heap_replace(heap_t *h, void *item) {
    __heap_replacex(h, item);
}

void *heap_peek(const heap_t *h) {
    if (0 == heap_count(h))
        return NULL;

    return h->array[0];
}

void heap_clear(heap_t *h) {
    h->count = 0;
}

void heap_clear_free_items(heap_t *h, void (*free_func)(void *)) {
    size_t n = heap_count(h);
    for (size_t i = 0; i < n; ++i) {
        free_func(h->array[i]);
    }

    h->count = 0;
}

/**
 * @return item's index on the heap's array; otherwise -1 */
static int __item_get_idx(const heap_t *h, const void *item) {
    unsigned int idx;

    for (idx = 0; idx < h->count; idx++)
        if (0 == h->cmp(h->array[idx], item, h->udata))
            return idx;

    return -1;
}

void *heap_remove_item(heap_t *h, const void *item) {
    int idx = __item_get_idx(h, item);

    if (idx == -1)
        return NULL;

    /* swap the item we found with the last item on the heap */
    void *ret_item = h->array[idx];
    h->array[idx] = h->array[h->count - 1];
    h->array[h->count - 1] = NULL;

    h->count -= 1;

    /* ensure heap property */
    if (idx < h->count) {
        // try to push up first
        int new_idx = __pushup(h, idx);
        // try to push down
        __pushdown(h, new_idx);
    } // else: we removed the last element in the array no need to pushup

    return ret_item;
}

int heap_contains_item(const heap_t *h, const void *item) {
    return __item_get_idx(h, item) != -1;
}

int heap_count(const heap_t *h) {
    return h->count;
}

int heap_size(const heap_t *h) {
    return h->size;
}

void _heap_cb_child(unsigned int idx, const heap_t *h, HeapCallback cb, void *ctx) {
    if (idx >= h->count)
        return;

    if (h->cmp(h->array[0], h->array[idx], h->udata) == 0) {
        cb(ctx, h->array[idx]);
        _heap_cb_child(__child_left(idx), h, cb, ctx);
        _heap_cb_child(__child_right(idx), h, cb, ctx);
    }
}

void heap_cb_root(const heap_t *hp, HeapCallback cb, void *ctx) {
    void *root = heap_peek(hp);
    if (!root)
        return;

    cb(ctx, root);

    _heap_cb_child(__child_left(0), hp, cb, ctx);
    _heap_cb_child(__child_right(0), hp, cb, ctx);
}

// itemWriteRDB: cb func to write the array's items to rdb
void heapRDBWrite(heap_t *h, RedisModuleIO *io, void (*itemWriteRDB_cb)()) {
    RedisModule_SaveUnsigned(io, h->size);
    RedisModule_SaveUnsigned(io, h->count);
    _log_if(h->udata != NULL, "Currently saving heap's user data to rdb is not supported.");
    for (size_t i = 0; i < h->count; ++i) {
        itemWriteRDB_cb(h->array[i], io);
    }
}

/*--------------------------------------------------------------79-characters-*/
