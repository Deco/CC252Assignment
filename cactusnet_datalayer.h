
//#include "cactusnet.h"

#ifndef CACTUSNET_DATALAYER_H
#define CACTUSNET_DATALAYER_H

const char cactusnet_FRAME_PREAMBLE[4] = {0x5E, 0x5E, 0x5E, 0xFF};

typedef enum cactusnet_FrameType_e {
    cactusnet_FrameType_DATA0 = 0,
    cactusnet_FrameType_DATA1    ,
    cactusnet_FrameType_ACK0     , // Acknowledge DATA1, please send DATA0
    cactusnet_FrameType_ACK1     , // Acknowledge DATA0, please send DATA1
    cactusnet_FrameType_e_max
} cactusnet_FrameType;

typedef struct cactusnet_Frame_Header_s {
    char preamble[4]; // 0x5E 0x5E 0x5E 0xFF
    cactusnet_FrameType type;
    uint32_t checksum; // not considered in checksum itself! (=zero)
} cactusnet_Frame_Header;

int cactusnet_makeframe(
    cactusnet_FrameType type,
    char *packet_it, size_t packet_size, // contained data from higher layers
    char **frame, size_t *frame_size // frame data to be sent over wire
);

int cactusnet_parseframe(
    char *frame, size_t frame_size,
    cactusnet_FrameType *type,
    char **p_packet, size_t *p_packet_size,
    bool *errordetected,
    uint32_t *ackchecksum
);
int cactusnet_data_process(    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    bool *next_scheduled,
    CnetTime *next_eta);
int cactusnet_data_send(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *packet, size_t packet_size
);
int cactusnet_data_rawsend(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    bool isack,
    bool duplicate,
    char *packet, size_t packet_size
);
int cactusnet_data_rawreceive(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *frame, size_t frame_size
);
int cactusnet_data_makeframe(
    cactusnet_FrameType type,
    char *packet, size_t packet_size,
    char **p_frame, size_t *p_frame_size
);
int cactusnet_data_parseframe(
    char *frame, size_t frame_size,
    cactusnet_FrameType *type,
    char **p_packet, size_t *p_packet_size,
    bool *errordetected,
    uint32_t *csreceived,
    uint32_t *cscomputed
);

#endif

