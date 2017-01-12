
//#include "cactusnet.h"
//#include "cactusnet_datalayer.c"
//#include "cactusnet_netlayer.c"

void cactusnet_NodeOptions_init(cactusnet_NodeOptions *obj) {
    obj->physicalsendcb = NULL;
    obj->transportreceivecb = NULL;
    obj->nodecount = 0;
    obj->nlinks = 0;
}

int cactusnet_init(
    cactusnet_NodeState *state,
    cactusnet_NodeOptions options,
    int nodeid,
    CnetAddr addr
  ) {
    state->options = options;
    state->nodeid = nodeid;
    state->addr = addr;
    state->linkarray = calloc(options.nlinks, sizeof(cactusnet_LinkState*));
    state->knownnodesvector = vector_new();
    state->routetable_size = 2;
    state->routetable = hashtable_new(options.nodecount+1);
        vector_append(state->knownnodesvector, &nodeinfo.address, sizeof(CnetAddr));
    
    char *addrkey = cactusnet_getnodekey(nodeinfo.address);
    cactusnet_RouteInfo selfroute;
    selfroute.addr = nodeinfo.address;
    selfroute.distance = 0;
    selfroute.locallinkid = 0;
    hashtable_add(        state->routetable,
        addrkey,
        &selfroute,
        sizeof(selfroute)    );
    free(addrkey);
    return 0;
}

int cactusnet_destroy(    cactusnet_NodeState *state  ) {   // We'll never have to call this, as CNet never does a cleanup!
   return 0;}

int cactusnet_process(
    cactusnet_NodeState *state,
    CnetTime time,
    bool *next_scheduled,
    CnetTime *next_eta
  ) {    *next_scheduled = false;    *next_eta = 9999999;    dbg("Processing");
    for(int link_i = 1; link_i < state->options.nlinks; link_i++) {
        cactusnet_LinkState *linkstate = state->linkarray[link_i];
        bool link_scheduled;
        CnetTime link_eta;
        cactusnet_data_process(state, linkstate, &link_scheduled, &link_eta);
        if(link_scheduled) {            *next_scheduled = true;
            if(link_eta < *next_eta) {                *next_eta = link_eta;            }        }
    }
    return 0;
}

int cactusnet_onlinkup(
    cactusnet_NodeState *state,
    int linkid
  ) {
    cactusnet_LinkState *linkstate = malloc(sizeof(cactusnet_LinkState));
    MEMCHECK(linkstate);
    state->linkarray[linkid] = linkstate;
    
    linkstate->linkid = linkid;
    linkstate->status = cactusnet_LinkState_Status_DISCONNECTED;
    //linkstate->routetable = hashtable_new();
    linkstate->receive_status = cactusnet_LinkState_ReceiveStatus_OK;
    linkstate->receive_seq = 1;
    linkstate->receive_acksent = false;
    linkstate->receive_prevtime = 0; 
    linkstate->send_seq = 1;
    linkstate->send_ackreceived = true;
    linkstate->send_buffer = queue_new();
    
    char *packet; size_t packet_size;
    cactusnet_net_makepacket_Introduction(
        &packet, &packet_size,
        state->addr // localaddr
    );
    
    cactusnet_data_send(state, linkstate, packet, packet_size);
    
    linkstate->status = cactusnet_LinkState_Status_HANDSHAKING;
    
    return 0;
}

int cactusnet_onlinkdown(
    cactusnet_NodeState *state,
    int linkid
  ) {
    // Remove from link list
    // NYI!
    
    return 0;
}

int cactusnet_physical_receive(
    cactusnet_NodeState *state,
    int linkid,
    void *frame, size_t frame_size
  ) {    cactusnet_LinkState *linkstate = state->linkarray[linkid];
    if(linkstate == NULL) RESCHECK(-1);    
    cactusnet_data_rawreceive(state, linkstate, frame, frame_size);
    
    return 0;
}

int cactusnet_physical_send(    cactusnet_NodeState *state,
    int linkid,
    void *frame, size_t frame_size  ) {    state->options.physicalsendcb(linkid, frame, &frame_size);
    return 0;}

int cactusnet_transport_receive(    cactusnet_NodeState *state,
    CnetAddr srcaddr,
    void *msg, size_t msg_size  ) {    state->options.transportreceivecb(srcaddr, msg, &msg_size);
    return 0;}

int cactusnet_transport_send(
    cactusnet_NodeState *state,
    CnetAddr destaddr,
    void *msg, size_t msg_size
  ) {
    
    cactusnet_net_send(        state,
        destaddr,
        msg, msg_size    );
    
    return 0;
}

char *cactusnet_getnodekey(CnetAddr addr) {    char *key;
    asprintf(&key, "N%d", (int)addr);
    dbg("key = %s", key);
    return key;}

void _dbg(int line, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    // Rather than redirect stdout, CNET overrides printf.
    // This means that vsprintf doesn't work correctly!
    //vprintf(fmt, args);
    char *str;
    vasprintf(&str, fmt, args); // this works nicely, though
    va_end(args);
    printf("%-10s[%d:L%03d]: %s\n", nodeinfo.nodename, nodeinfo.nodenumber, line, str);
    fprintf(stderr, "%-10s[%d:L%03d]: %s\n", nodeinfo.nodename, nodeinfo.nodenumber, line, str);
    fflush(stderr);
    free(str);
}

