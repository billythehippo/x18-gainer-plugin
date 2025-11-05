#ifndef X18_H_
#define X18_H_

typedef struct
{
    uint32_t ip;
    uint16_t port;
    uint16_t flags;
    float osc_gain[17];
    float old_gain[17];
} x18_context_t;
#define ANSWERED  0x0001
#define FBENABLED 0x0002

#endif
