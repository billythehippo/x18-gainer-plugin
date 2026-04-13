#ifndef X18_H_
#define X18_H_

#include <cstdint>
typedef struct
{
    uint32_t ip;
    uint16_t port;
    uint16_t flags;
    uint16_t phantomsOld;
    uint16_t phantomsCur;
    uint8_t linksOld;
    uint8_t linksCur;
    float osc_gain[17];
    float old_gain[17];
} x18_context_t;
#define CONNECTED 0x0001
#define ANSWERED  0x0002
#define FBENABLED 0x0004

#endif
