#pragma once

#include "vec.h"

#define VEC2OP(type, ptype, name, op)                                    \
  CT_EXPORT ct_inline type *name##v_imm(type *a, type *b) {              \
    a->x op## = b->x;                                                    \
    a->y op## = b->y;                                                    \
    return a;                                                            \
  }                                                                      \
                                                                         \
  CT_EXPORT ct_inline type *name##v(type *a, type *b, CT_MPool *mpool) { \
    type *v = ALLOCATE_TYPE(mpool, type);                                \
    v->x = a->x op b->x;                                                 \
    v->y = a->y op b->y;                                                 \
    return v;                                                            \
  }                                                                      \
                                                                         \
  CT_EXPORT ct_inline type *name##n_imm(type *v, ptype n) {              \
    v->x op## = n;                                                       \
    v->y op## = n;                                                       \
    return v;                                                            \
  }                                                                      \
                                                                         \
  CT_EXPORT ct_inline type *name##n(type *a, ptype n, CT_MPool *mpool) { \
    type *v = ALLOCATE_TYPE(mpool, type);                                \
    v->x = a->x op n;                                                    \
    v->y = a->y op n;                                                    \
    return v;                                                            \
  }                                                                      \
                                                                         \
  CT_EXPORT ct_inline type *name##xy_imm(type *v, ptype x, ptype y) {    \
    v->x op## = x;                                                       \
    v->y op## = y;                                                       \
    return v;                                                            \
  }                                                                      \
                                                                         \
  CT_EXPORT ct_inline type *name##xy(type *a, ptype x, ptype y,          \
                                     CT_MPool *mpool) {                  \
    type *v = ALLOCATE_TYPE(mpool, type);                                \
    v->x = a->x op x;                                                    \
    v->y = a->y op y;                                                    \
    return v;                                                            \
  }

typedef union {
  struct {
    float x, y;
  };
  float buf[2];
} CT_Vec2f;

VEC2OP(CT_Vec2f, float, ct_add2f, +)
VEC2OP(CT_Vec2f, float, ct_sub2f, -)
VEC2OP(CT_Vec2f, float, ct_mul2f, *)
VEC2OP(CT_Vec2f, float, ct_div2f, /)

CT_EXPORT CT_Vec2f *ct_vec2f(float x, float y, CT_MPool *mpool);
CT_EXPORT CT_Vec2f *ct_vec2fn(float n, CT_MPool *mpool);

CT_EXPORT ct_inline CT_Vec2f *ct_set2fv(CT_Vec2f *a, CT_Vec2f *b) {
  a->x = b->x;
  a->y = b->y;
  return a;
}

CT_EXPORT ct_inline CT_Vec2f *ct_set2fxy(CT_Vec2f *v, float x, float y) {
  v->x = x;
  v->y = y;
  return v;
}

CT_EXPORT ct_inline uint8_t ct_deltaeq2fv(CT_Vec2f *a, CT_Vec2f *b, float eps) {
  return (ct_deltaeqf(a->x, b->x, eps) && ct_deltaeqf(a->y, b->y, eps));
}

CT_EXPORT ct_inline CT_Vec2f *ct_madd2fv_imm(CT_Vec2f *a, CT_Vec2f *b,
                                             CT_Vec2f *c) {
  a->x = a->x * b->x + c->x;
  a->y = a->y * b->y + c->y;
  return a;
}

CT_EXPORT ct_inline CT_Vec2f *ct_madd2fv(CT_Vec2f *a, CT_Vec2f *b, CT_Vec2f *c,
                                         CT_MPool *mpool) {
  return ct_madd2fv_imm(ct_set2fv(ALLOCATE_TYPE(mpool, CT_Vec2f), a), b, c);
}

CT_EXPORT ct_inline float ct_dot2fv(CT_Vec2f *a, CT_Vec2f *b) {
  return a->x * b->x + a->y * b->y;
}

CT_EXPORT ct_inline float ct_distsq2fv(CT_Vec2f *a, CT_Vec2f *b) {
  float dx = a->x - b->x;
  float dy = a->y - b->y;
  return dx * dx + dy * dy;
}

CT_EXPORT ct_inline float ct_dist2fv(CT_Vec2f *a, CT_Vec2f *b) {
  return sqrtf(ct_distsq2fv(a, b));
}

CT_EXPORT ct_inline float ct_magsq2f(CT_Vec2f *v) {
  return v->x * v->x + v->y * v->y;
}

CT_EXPORT ct_inline float ct_mag2f(CT_Vec2f *v) {
  return sqrtf(v->x * v->x + v->y * v->y);
}

CT_EXPORT ct_inline CT_Vec2f *ct_mix2fv_imm(CT_Vec2f *a, CT_Vec2f *b, float t) {
  a->x = ct_mixf(a->x, b->x, t);
  a->y = ct_mixf(a->y, b->y, t);
  return a;
}

CT_EXPORT ct_inline CT_Vec2f *ct_mix2fv(CT_Vec2f *a, CT_Vec2f *b, float t,
                                        CT_MPool *mpool) {
  return ct_mix2fv_imm(ct_set2fv(ALLOCATE_TYPE(mpool, CT_Vec2f), a), b, t);
}

CT_EXPORT ct_inline CT_Vec2f *ct_normalize2f_imm(CT_Vec2f *v, float len) {
  float m = ct_mag2f(v);
  if (m > 0.0) {
    len /= m;
    v->x *= len;
    v->y *= len;
  }
  return v;
}

CT_EXPORT ct_inline CT_Vec2f *ct_normalize2f(CT_Vec2f *v, float len,
                                             CT_MPool *mpool) {
  return ct_normalize2f_imm(ct_set2fv(ALLOCATE_TYPE(mpool, CT_Vec2f), v), len);
}

CT_EXPORT ct_inline uint8_t ct_is_normalized2f(CT_Vec2f *v) {
  return ct_deltaeqf(ct_mag2f(v) - 1.f, 0.f, EPS);
}