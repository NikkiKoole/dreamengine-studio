#ifndef DE_CART_CTX_H
#define DE_CART_CTX_H
// cart_ctx.h — the shared half of "a cart-land header's state can be PER-INSTANCE".
//
// WHY THIS EXISTS. An AUv3 puts every plug-in instance in one process, and a cart is ONE translation
// unit — so two racks running the same cart share everything the cart and its headers declare
// `static`. The engine's own state is already per-instance (docs/design/engine-context.md); this is
// the cart-land half. A rack whose widget table or drum tuning is shared is not a second rack.
//
// HOW A HEADER USES IT. Declare the state ONCE as an X-list, then fork:
//
//     #define FOO_STATE(X)                       \
//         X(int,   foo_n,   ,          0)        \
//         X(float, foo_buf, [FOO_MAX], {0})
//
//     #ifndef DE_CART_CTX
//     DE_CTX_STATICS(FOO_STATE)                 // DEFAULT: exactly the statics that were there
//     #else
//     static void foo_ctx_init_(struct FooCtx *c);   // your non-zero defaults (often empty)
//     DE_CTX_BLOCK(foo, Foo, FOO_STATE)              // struct + key + accessor
//     #define foo_n   (foo_ctx_()->foo_n)             // one per name — the preprocessor cannot
//     #define foo_buf (foo_ctx_()->foo_buf)           // generate these, so they are written out
//     #endif
//
// The DEFAULT expansion is the point: it produces the same declarations the header had before, so
// every cart that does not opt in compiles to the same thing and pays nothing. Only a cart that
// defines DE_CART_CTX — a plug-in rack that can be loaded twice — takes the context path.
//
// ⚠ THE ACCESSOR IS CALLED ON EVERY ACCESS, NEVER CACHED. Registering another header's key can grow
// the state block and move every slice (see de_state_for in studio.h). Caching a pointer across
// calls is a use-after-realloc waiting for the second header to appear.
//
// Gated by tools/instance-check/run-uictx.sh, which builds BOTH paths and asserts opposite things:
// the default must stay shared, the opted-in must not. Checking only the opt-in would leave the path
// every cart compiles unverified.

// the four expansions a list gets put through
#define DE_CTX_DECL_(t, n, d, i)   static t n d = i;
#define DE_CTX_MEMBER_(t, n, d, i) t n d;

// DEFAULT path: plain file-scope statics, byte-identical to hand-written ones.
#define DE_CTX_STATICS(LIST) LIST(DE_CTX_DECL_)

// OPT-IN path: the same list as a struct, plus the accessor that finds this instance's copy.
// `lc` = lowercase prefix (symbol names), `Uc` = the struct's name.
// The key is the ADDRESS of a file-scope sentinel this header owns: unique per translation unit by
// construction, so headers cannot collide and there is no slot registry to keep in sync.
#define DE_CTX_BLOCK(lc, Uc, LIST)                                              \
    typedef struct Uc##Ctx { LIST(DE_CTX_MEMBER_) int de_ctx_inited_; } Uc##Ctx; \
    static char lc##_ctx_key_;                                                  \
    static void lc##_ctx_init_(Uc##Ctx *c);                                     \
    static Uc##Ctx *lc##_ctx_(void) {                                           \
        Uc##Ctx *c = (Uc##Ctx *)de_state_for(&lc##_ctx_key_, (int)sizeof(Uc##Ctx)); \
        if (c && !c->de_ctx_inited_) { c->de_ctx_inited_ = 1; lc##_ctx_init_(c); } \
        return c;                                                               \
    }

#endif
