#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/dbg.h"
#include "data/hashtable.h"
#include "math/ct_math.h"

static int make_key(CT_Hashtable* t, CT_HTEntry* e, void* key, size_t ks) {
  if (t->flags & CT_HT_CONST_KEYS) {
    e->key = key;
  } else {
    if (t->ops.alloc_key != NULL) {
      e->key = t->ops.alloc_key(ks, t->ops.state);
    } else {
      e->key = malloc(ks);
    }
    if (!e->key) return 1;
    memcpy(e->key, key, ks);
  }
  e->keySize = ks;
  return 0;
}

static int make_val(CT_Hashtable* t, CT_HTEntry* e, void* val, size_t vs) {
  if (t->flags & CT_HT_CONST_VALS) {
    e->val = val;
  } else {
    void* p;
    if (t->ops.alloc_val != NULL) {
      p = t->ops.alloc_val(vs, t->ops.state);
    } else {
      p = malloc(vs);
    }
    if (p == NULL) return 1;
    memcpy(p, val, vs);
    e->val = p;
  }
  e->valSize = vs;
  return 0;
}

static CT_HTEntry* make_entry(CT_Hashtable* t, void* key, void* val, size_t ks,
                              size_t vs) {
  CT_HTEntry* e = ct_mpool_alloc(&t->pool);
  CT_CHECK_MEM(e);
  e->next = NULL;
  if (!make_key(t, e, key, ks) && !make_val(t, e, val, vs)) {
    CT_DEBUG("new HTEntry: %p, k=%p, v=%p", e, e->key, e->val);
    return e;
  }
fail:
  return NULL;
}

static void free_key(CT_Hashtable* t, CT_HTEntry* e) {
  CT_DEBUG("free key: %p", e->key);
  if (t->ops.free_key != NULL) {
    t->ops.free_key(e->key, t->ops.state);
  } else {
    free(e->key);
  }
}

static void free_val(CT_Hashtable* t, CT_HTEntry* e) {
  CT_DEBUG("free val: %p", e->val);
  if (t->ops.free_val != NULL) {
    t->ops.free_val(e->val, t->ops.state);
  } else {
    free(e->val);
  }
}

static void free_entry(CT_Hashtable* t, CT_HTEntry* e) {
  CT_DEBUG("free HTEntry: %p", e);
  if (!(t->flags & CT_HT_CONST_KEYS)) {
    free_key(t, e);
  }
  if (!(t->flags & CT_HT_CONST_VALS)) {
    free_val(t, e);
  }
  ct_mpool_free(&t->pool, e);
}

static int equiv_keys(const void* a, const void* b, size_t sa, size_t sb) {
  return sa == sb ? !memcmp(a, b, sa) : 0;
}

static CT_HTEntry* find_entry(CT_Hashtable* t, CT_HTEntry* e, void* key,
                              size_t ks) {
  int (*equiv_keys)(const void*, const void*, size_t, size_t) =
      t->ops.equiv_keys;
  while (e != NULL) {
    CT_DEBUG("find e: %p", e);
    if (equiv_keys(key, e->key, ks, e->keySize)) {
      return e;
    }
    e = e->next;
  }
  return e;
}

static int cons_entry(CT_Hashtable* t, uint32_t bin, void* key, void* val,
                      size_t ks, size_t vs) {
  // TODO resize table?
  CT_HTEntry* e = make_entry(t, key, val, ks, vs);
  CT_CHECK_MEM(e);
  e->next = t->bins[bin];
  t->bins[bin] = e;
fail:
  return e == NULL;
}

static void delete_entry(CT_Hashtable* t, uint32_t bin, CT_HTEntry* e) {
  CT_HTEntry* first = t->bins[bin];
  CT_HTEntry* rest = first->next;
  if (first == e) {
    t->bins[bin] = rest;
    free_entry(t, first);
    if (rest != NULL) {
      t->collisions--;
    }
  } else {
    while (rest != NULL) {
      if (rest == e) {
        first->next = e->next;
        free_entry(t, e);
        t->collisions--;
        return;
      }
      first = rest;
      rest = rest->next;
    }
  }
}

CT_EXPORT int ct_ht_init(CT_Hashtable* t, CT_HTOps* ops, size_t num,
                         size_t poolSize, CT_HTFlags flags) {
  int mp = ct_mpool_init(&t->pool, poolSize, sizeof(CT_HTEntry));
  if (!mp) {
    num = ct_ceil_pow2(num);
    CT_INFO("HT bin count: 0x%zx", num);
    t->bins = calloc(num, sizeof(CT_HTEntry*));
    CT_CHECK_MEM(&t->bins);
    t->ops = *ops;
    if (t->ops.equiv_keys == NULL) {
      t->ops.equiv_keys = equiv_keys;
    }
    t->binMask = num - 1;
    t->flags = flags;
    t->size = 0;
    t->collisions = 0;
  }
fail:
  return mp || t->bins == NULL;
}

CT_EXPORT void ct_ht_free(CT_Hashtable* t) {
  const size_t freeK = !(t->flags & CT_HT_CONST_KEYS);
  const size_t freeV = !(t->flags & CT_HT_CONST_VALS);
  if (freeK || freeV) {
    for (size_t i = 0; i <= t->binMask; i++) {
      CT_HTEntry* e = t->bins[i];
      while (e != NULL) {
        if (freeK) free_key(t, e);
        if (freeV) free_val(t, e);
        e = e->next;
      }
    }
  }
  ct_mpool_free_all(&t->pool);
  CT_DEBUG("free HT bins: %p", t->bins);
  free(t->bins);
  t->bins = NULL;
}

CT_EXPORT int ct_ht_assoc(CT_Hashtable* t, void* key, void* val, size_t ks,
                          size_t vs) {
  uint32_t hash = t->ops.hash(key, ks);
  uint32_t bin = hash & t->binMask;
  CT_HTEntry* e = t->bins[bin];
  if (e == NULL) {
    CT_DEBUG("new entry w/ hash: %x, bin: %x", hash, bin);
    e = make_entry(t, key, val, ks, vs);
    CT_CHECK_MEM(e);
    t->bins[bin] = e;
    t->size++;
  } else {
    e = find_entry(t, e, key, ks);
    if (e != NULL) {
      CT_DEBUG("overwrite %p w/ hash: %x, bin: %x", e, hash, bin);
      if (!(t->flags & CT_HT_CONST_VALS)) {
        free_val(t, e);
      }
      return make_val(t, e, val, vs);
    } else {
      CT_DEBUG("hash coll (hash: %x, bin: %x), push new entry...", hash, bin);
      if (cons_entry(t, bin, key, val, ks, vs)) {
        return 1;
      }
      t->collisions++;
      t->size++;
    }
  }
  return 0;
fail:
  return 1;
}

CT_EXPORT void* ct_ht_get(CT_Hashtable* t, void* key, size_t ks, size_t* vs) {
  uint32_t bin = t->ops.hash(key, ks) & t->binMask;
  CT_HTEntry* e = t->bins[bin];
  if (e != NULL) {
    e = find_entry(t, e, key, ks);
  }
  if (e != NULL) {
    if (vs != NULL) {
      *vs = e->valSize;
    }
    return e->val;
  }
  return NULL;
}

CT_EXPORT int ct_ht_contains(CT_Hashtable* t, void* key, size_t ks) {
  uint32_t bin = t->ops.hash(key, ks) & t->binMask;
  CT_HTEntry* e = t->bins[bin];
  if (e != NULL) {
    e = find_entry(t, e, key, ks);
  }
  return (e != NULL);
}

CT_EXPORT int ct_ht_dissoc(CT_Hashtable* t, void* key, size_t ks) {
  uint32_t bin = t->ops.hash(key, ks) & t->binMask;
  CT_HTEntry* e = t->bins[bin];
  if (e != NULL) {
    e = find_entry(t, e, key, ks);
    if (e != NULL) {
      delete_entry(t, bin, e);
      t->size--;
      return 0;
    }
  }
  return 1;
}

CT_EXPORT void* ct_ht_update(CT_Hashtable* t, void* key, size_t ks,
                             CT_HTUpdater update, void* state) {
  uint32_t bin = t->ops.hash(key, ks) & t->binMask;
  CT_HTEntry* e = t->bins[bin];
  if (e != NULL) {
    e = find_entry(t, e, key, ks);
  }
  if (e != NULL) {
    // TODO mutate or free old & create new val
    update(&e->val, &e->valSize, state);
    return e->val;
  }
  return NULL;
}

CT_EXPORT int ct_ht_iterate(CT_Hashtable* t, CT_HTVisitor iter, void* state) {
  for (size_t i = 0; i <= t->binMask; i++) {
    CT_HTEntry* e = t->bins[i];
    while (e != NULL) {
      int res = iter(e, state);
      if (res) return res;
      e = e->next;
    }
  }
  return 0;
}

static int ct_ht_into_iter(CT_HTEntry* e, void* t) {
  return ct_ht_assoc((CT_Hashtable*)t, e->key, e->val, e->keySize, e->valSize);
}

CT_EXPORT int ct_ht_into(CT_Hashtable* dest, CT_Hashtable* src) {
  return ct_ht_iterate(src, ct_ht_into_iter, dest);
}
