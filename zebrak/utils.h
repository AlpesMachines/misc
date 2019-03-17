#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define ALWAYS_INLINE __attribute__((always_inline))
#define STATIC_ASSERT(cond,msg) typedef char static_assertion_##msg[(cond)?1:-1]
