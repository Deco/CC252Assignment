
//#include "cactusnet.h"

#ifndef CACTUSNET_NETLAYER_H
#define CACTUSNET_NETLAYER_H

typedef enum cactusnet_PacketType_e {
    cactusnet_PacketType_NET_INTRO      = 0,
    cactusnet_PacketType_NET_UPDATE        ,
    cactusnet_PacketType_TRANSPORT_DATA    ,
} cactusnet_PacketType;

typedef struct cactusnet_Packet_Header_s {
    cactusnet_PacketType type;
} cactusnet_Packet_Header;


typedef struct cactusnet_Packet_Introduction_s {
    cactusnet_Packet_Header header;
    CnetAddr localaddr;
} cactusnet_Packet_Introduction;

int cactusnet_net_makepacket_Introduction(
    char **p_packet, size_t *p_packet_size,
    CnetAddr localaddr
);
int cactusnet_net_parsepacket_Introduction(
    char *packet, size_t packet_size,
    cactusnet_Packet_Introduction **p_intro
);

typedef struct cactusnet_Packet_NetUpdate_s {
    cactusnet_Packet_Header header;
    int linkcost;
    int routetable_count;
    cactusnet_RouteInfo routetable[0]; // zero-sized array used as pointer
} cactusnet_Packet_NetUpdate;

int cactusnet_net_makepacket_NetUpdate(
    char **p_packet, size_t *p_packet_size,
    int linkcost,
    cactusnet_LinkStateVector nodelist,
    cactusnet_RouteInfoTable routetable
);
int cactusnet_net_parsepacket_NetUpdate(
    char *packet, size_t packet_size,
    cactusnet_Packet_NetUpdate **p_update
);

typedef struct cactusnet_Packet_TransportData_s {
    cactusnet_Packet_Header header;
    CnetAddr srcaddr;
    CnetAddr destaddr;
    size_t data_size;
    char data[0]; // zero-sized array used as pointer to tailing data
} cactusnet_Packet_TransportData;

int cactusnet_net_makepacket_TransportData(
    char **p_packet, size_t *p_packet_size,
    CnetAddr srcaddr,
    CnetAddr destaddr,
    char *data, size_t data_size
);
int cactusnet_net_parsepacket_TransportData(
    char *packet, size_t packet_size,
    cactusnet_Packet_TransportData **p_data
);


int cactusnet_net_send(    cactusnet_NodeState *state,
    CnetAddr destaddr,
    void *msg, size_t msg_size
);
int cactusnet_net_handlepacket(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *packet, size_t packet_size
);

#endif

