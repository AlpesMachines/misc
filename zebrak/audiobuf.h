#define N_OUTS 2
struct outs {
        byte a, b;
};
#define PREFIX(x) audiobuf_ ## x
#define ELEM struct outs
#define SIZE 256u
#define VOLATILE
#include "ring.h"
