struct context;
typedef struct context *context;
typedef u64 *context_frame;
typedef closure_type(fault_handler, context, context);

#define CONTEXT_TYPE_UNDEFINED 0
#define CONTEXT_TYPE_KERNEL    1
#define CONTEXT_TYPE_SYSCALL   2
#define CONTEXT_TYPE_THREAD    3

struct context {
    u64 frame[FRAME_SIZE]; /* must be first */
    thunk pause;
    thunk resume;
    fault_handler fault_handler;
    heap transient_heap;
    u8 type;
};