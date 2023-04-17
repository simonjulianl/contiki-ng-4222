#ifndef NBR_CONST_H
#define NBR_CONST_H

#define NUM_SEND 2

typedef enum {
    LIGHT_SENSOR_REQUESTER,
    LIGHT_SENSOR_PROVIDER
} Role;

typedef enum {
    CONNECTING_PACKET,
    LIGHT_SENSOR_PACKET
} Type;

typedef struct __attribute__((packed)) {
    unsigned long src_id;
    unsigned long timestamp; // timestamp and seq are used for debugging
    unsigned long seq;
    Role role;
    Type type;
    unsigned long data[10];
} data_packet_struct;
/*---------------------------------------------------------------------------*/

#define RSSI_THRESHOLD (-70) // based on heuristics
#define CONNECT_PERIOD 15
#define CONNECT_INTERVAL 1
#define DISCONNECT_PERIOD 30
#define SAMPLING_RATE 30
#define EMPTY_ID (-1)

#endif //NBR_CONST_H
