
//#include "cactusnet_datalayer.h"
//#include "cactusnet_netlayer.h"

int cactusnet_net_send(    cactusnet_NodeState *state,
    CnetAddr destaddr,
    void *msg, size_t msg_size  ) {
    char *packet; size_t packet_size;
    cactusnet_net_makepacket_TransportData(        &packet, &packet_size,
        state->addr,
        destaddr,
        msg, msg_size    );
    
    char *addrkey = cactusnet_getnodekey(destaddr);
    size_t routeinfo_size;
    cactusnet_RouteInfo *p_routeinfo = hashtable_find(        state->routetable,
        addrkey,
        &routeinfo_size    );
    free(addrkey);
    if(p_routeinfo) {        int linkid = p_routeinfo->locallinkid;
        cactusnet_LinkState *linkstate = state->linkarray[linkid];
        if(linkstate != NULL) {
            cactusnet_data_send(                state,
                linkstate,
                packet, packet_size            );
        } else {
            //RESCHECK(-1);
        }
        free(p_routeinfo);
    } else {        //CHECK(-1);    }
    
    return 0;}

int cactusnet_net_handlepacket(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *packet, size_t packet_size
  ) {
    cactusnet_Packet_Header *header = (cactusnet_Packet_Header*)packet;
    if(header->type == cactusnet_PacketType_NET_INTRO) {
        dbg("Received NET_INTRO from l%d", linkstate->linkid);        cactusnet_Packet_Introduction *p_intro;
        cactusnet_net_parsepacket_Introduction(                packet, packet_size,
                &p_intro        );
        
        linkstate->status = cactusnet_LinkState_Status_DISCONNECTED;
        linkstate->partner_addr = p_intro->localaddr;
                vector_append(state->knownnodesvector, &p_intro->localaddr, sizeof(CnetAddr));
        
        cactusnet_RouteInfo route;
        route.addr = p_intro->localaddr;
        route.distance = 1;
        route.locallinkid = linkstate->linkid;
        char *addrkey = cactusnet_getnodekey(p_intro->localaddr);
        hashtable_add(            state->routetable,
            addrkey,
            &route,
            sizeof(route)        );
        free(addrkey);
        
        char *newpacket; size_t newpacket_size;
        cactusnet_net_makepacket_NetUpdate(
            &newpacket, &newpacket_size,
            0, // linkcost
            state->knownnodesvector,
            state->routetable
        );
        dbg("Sending NET_UPDATE(%d) to l%d", newpacket_size, linkstate->linkid);
        cactusnet_data_send(state, linkstate, newpacket, newpacket_size);
        free(newpacket);    } else if(header->type == cactusnet_PacketType_NET_UPDATE) {
        dbg("Received NET_UPDATE from l%d", linkstate->linkid);        cactusnet_Packet_NetUpdate *p_update;
        cactusnet_net_parsepacket_NetUpdate(                packet, packet_size,
                &p_update        );
        VECTOR updatednodes = vector_new();
        dbg("RTC %d", p_update->routetable_count);
        for(int routeinfo_i = 0; routeinfo_i < p_update->routetable_count; routeinfo_i++) {            cactusnet_RouteInfo *p_routeinfo = &p_update->routetable[routeinfo_i];
            dbg("ri %d", p_routeinfo->addr);
            bool nodeknown = false;
            for(int node_i = 0; node_i < vector_nitems(state->knownnodesvector); node_i++) {
                size_t node_addr_size; // not needed at all, but it's how the vector library works!
                CnetAddr *p_node_addr = vector_peek(state->knownnodesvector, node_i, &node_addr_size);
                if(p_node_addr) {
                    if(*p_node_addr == p_routeinfo->addr) {                        nodeknown = true;
                        //break;                    }
                    free(p_node_addr);
                } else {                    CHECK(-1);                }
            }
                        cactusnet_RouteInfo *p_localrouteinfo = NULL; 
            char *addrkey = cactusnet_getnodekey(p_routeinfo->addr);
            if(nodeknown) {
                size_t localrouteinfo_size;                p_localrouteinfo = hashtable_find(                    state->routetable,
                    addrkey,
                    &localrouteinfo_size
                );
            } else {                vector_append(state->knownnodesvector, &p_routeinfo->addr, sizeof(CnetAddr));
                CNET_enable_application(p_routeinfo->addr);
                dbg("Discovered node @%d", p_routeinfo->addr);            }
            if(!nodeknown || p_routeinfo->distance < (*p_localrouteinfo).distance) {                p_routeinfo->distance++;
                p_routeinfo->locallinkid = linkstate->linkid;                hashtable_add(                    state->routetable,
                    addrkey,
                    p_routeinfo,
                    sizeof(cactusnet_RouteInfo)
                );
                vector_append(updatednodes, &p_routeinfo->addr, sizeof(CnetAddr));
            }
            if(p_localrouteinfo) free(p_localrouteinfo);
            free(addrkey);
        }
        for(int link_i = 1; link_i < state->options.nlinks; link_i++) {
            cactusnet_LinkState *neighbourstate = state->linkarray[link_i];
            
            char *newpacket; size_t newpacket_size;
            cactusnet_net_makepacket_NetUpdate(
                &newpacket, &newpacket_size,
                0, // linkcost
                updatednodes,
                state->routetable
            );
            
            cactusnet_data_send(state, neighbourstate, newpacket, packet_size);
            free(newpacket);        }
        vector_free(updatednodes);    } else if(header->type == cactusnet_PacketType_TRANSPORT_DATA) {
        dbg("Received TRANSPORT_DATA from l%d", linkstate->linkid);        cactusnet_Packet_TransportData *p_data;
        cactusnet_net_parsepacket_TransportData(            packet, packet_size,            &p_data        );
        if(p_data->destaddr == state->addr) {            dbg("Received message from @%d (l%d)", p_data->srcaddr);
            state->options.transportreceivecb(p_data->srcaddr, p_data->data, &(p_data->data_size));        } else {            dbg("Forwarding message from @%d (l%d) to @%d",
                p_data->srcaddr, linkstate->linkid,
                p_data->destaddr
            );            cactusnet_net_send(                state,
                p_data->destaddr,
                p_data->data, p_data->data_size            );        }    } else {        CHECK(-1);    }
    
    return 0;}

int cactusnet_net_makepacket_Introduction(
    char **p_packet, size_t *p_packet_size,
    CnetAddr localaddr
  ) {
    *p_packet_size = sizeof(cactusnet_Packet_Introduction);
    
    char *packet_it = malloc(*p_packet_size);
    *p_packet = packet_it;
    
    cactusnet_Packet_Introduction *header = (cactusnet_Packet_Introduction*)(packet_it);
    header->header.type = cactusnet_PacketType_NET_INTRO;
    header->localaddr = localaddr;
    
    return 0;
}
int cactusnet_net_parsepacket_Introduction(
    char *packet, size_t packet_size,
    cactusnet_Packet_Introduction **intro
  ) {
    
    *intro = (cactusnet_Packet_Introduction*)packet; // surprisingly easy!
    
    return 0;
}

int cactusnet_net_makepacket_NetUpdate(
    char **p_packet, size_t *p_packet_size,
    int linkcost,
    cactusnet_LinkStateVector nodelist,
    cactusnet_RouteInfoTable routetable
  ) {
    *p_packet_size = (
            sizeof(cactusnet_Packet_NetUpdate)
        +   vector_nitems(nodelist)*sizeof(cactusnet_RouteInfo)
    );
    dbg("PS = %d", *p_packet_size);
    char *packet_it = malloc(*p_packet_size);
    *p_packet = packet_it;
    
    cactusnet_Packet_NetUpdate *header = (cactusnet_Packet_NetUpdate*)(packet_it);
    header->header.type = cactusnet_PacketType_NET_UPDATE;
    header->linkcost = linkcost;
    int header_routetable_count = 0;
    
    packet_it += sizeof(header);
    
    for(int node_i = 1; node_i < vector_nitems(nodelist); node_i++) {
        size_t node_addr_size; // not needed at all, but it's how the vector library works!
        CnetAddr *p_node_addr = vector_peek(nodelist, node_i, &node_addr_size);
        size_t routeinfo_size;
        char *addrkey = cactusnet_getnodekey(*p_node_addr);
        cactusnet_RouteInfo *p_routeinfo = hashtable_find(            routetable,
            addrkey,
            &routeinfo_size
        );
        free(addrkey);
        if(p_routeinfo) {
            memcpy(packet_it, p_routeinfo, routeinfo_size);
            packet_it += sizeof(cactusnet_RouteInfo);
            header_routetable_count++;
            free(p_routeinfo);
        } else {
            RESCHECK(-1); // shouldn't happen!
        }
        //free(p_node_addr);
    }
    header->routetable_count = header_routetable_count;
    dbg("?!? %d", header->routetable_count);
    
    return 0;}
int cactusnet_net_parsepacket_NetUpdate(
    char *packet, size_t packet_size,
    cactusnet_Packet_NetUpdate **p_update
  ) {    *p_update = (cactusnet_Packet_NetUpdate*)packet;
    dbg("!!! %d", (*p_update)->routetable_count);
    
    return 0;}

int cactusnet_net_makepacket_TransportData(
    char **p_packet, size_t *p_packet_size,
    CnetAddr srcaddr,
    CnetAddr destaddr,
    char *data, size_t data_size
  ) {    *p_packet_size = (
            sizeof(cactusnet_Packet_TransportData)
        +   data_size
    );
    char *packet_it = malloc(*p_packet_size);
    *p_packet = packet_it;
    
    cactusnet_Packet_TransportData *header = (cactusnet_Packet_TransportData*)(packet_it);
    header->header.type = cactusnet_PacketType_TRANSPORT_DATA;
    header->srcaddr = srcaddr;
    header->destaddr = destaddr;
    header->data_size = data_size;
    memcpy(header->data, data, data_size);
    
    return 0;}
int cactusnet_net_parsepacket_TransportData(
    char *packet, size_t packet_size,
    cactusnet_Packet_TransportData **p_data
  ) {    *p_data = (cactusnet_Packet_TransportData*)packet;
    
    return 0;}

