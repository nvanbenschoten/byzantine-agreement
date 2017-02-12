#ifndef MESSAGE_H_
#define MESSAGE_H_

typedef struct {
    uint32_t type;  // Must be equal to 1
    uint32_t size;  // size of message in bytes
    uint32_t round; // round number
    uint32_t order; // the order (retreat = 0 and attack = 1)
    uint32_t ids[]; // id’s of the senders of this message
} ByzantineMessage;

typedef struct {
    uint32_t type;  // Must be equal to 2
    uint32_t size;  // size of message in bytes
    uint32_t round; // round number
} Ack;

#endif
