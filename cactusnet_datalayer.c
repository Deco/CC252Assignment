
//#include "cactusnet_datalayer.h"
//#include "cactusnet_netlayer.h"

int cactusnet_data_process(    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    bool *next_scheduled,
    CnetTime *next_eta  ) {    *next_eta = 999999;
    if(!linkstate->send_ackreceived) {        CnetTime timenow = nodeinfo.time_in_usec;
        CnetTime timesent = linkstate->send_prevtime;
        if(timenow-timesent > linkstate->send_expectedackdelay) {            char *packet; size_t packet_size;
            packet = queue_peek(linkstate->send_buffer, &packet_size);            cactusnet_data_rawsend(                state,
                linkstate,
                false, true,
                packet, packet_size            );        } else {            *next_scheduled = true;
            if(timenow-timesent+linkstate->send_expectedackdelay < *next_eta) {                *next_eta = timenow-timesent+linkstate->send_expectedackdelay;            }        }    }
    dbg("Processing done");
    return 0;}

int cactusnet_data_send(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *packet, size_t packet_size
  ) {    dbg("Queueing frame to l%d (%d bytes)", linkstate->linkid, packet_size);    queue_add(linkstate->send_buffer, packet, packet_size);
        if(queue_nitems(linkstate->send_buffer) == 1) {        char *packet; size_t packet_size;
        packet = queue_peek(linkstate->send_buffer, &packet_size);
        //dbg("Sending %s to l%d", linkstate->linkid);
        dbg("PTS %p", packet);        cactusnet_data_rawsend(            state,
            linkstate,
            false, false,
            packet, packet_size        );
        //free(packet);    }
    return 0;}

int cactusnet_data_rawsend(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    bool isack,
    bool duplicate,
    char *packet, size_t packet_size
  ) {    cactusnet_FrameType type;
    if(!duplicate) {        if(isack) {            dbg("Sending ACK to l%d", linkstate->linkid);            type = (                  linkstate->receive_seq == 1 ? cactusnet_FrameType_ACK0
                : linkstate->receive_seq == 0 ? cactusnet_FrameType_ACK1
                : -1            );
            VALCHECK(type != -1);
            linkstate->receive_acksent = true;
            linkstate->receive_prevtime = nodeinfo.time_in_usec;        } else {            dbg("Sending DATA to l%d", linkstate->linkid);            type = (                  linkstate->send_seq == 1 ? cactusnet_FrameType_DATA0
                : linkstate->send_seq == 0 ? cactusnet_FrameType_DATA1
                : -1            );
            VALCHECK(type != -1);
            linkstate->send_seq = 1-linkstate->send_seq;
            linkstate->send_ackreceived = false;            linkstate->send_prevtime = nodeinfo.time_in_usec;
        }
    } else {        dbg("Resending DATA to l%d", linkstate->linkid);
    }
    
    char *frame; size_t frame_size;
    dbg("PTS %p", packet);
    cactusnet_data_makeframe(        type,        packet, packet_size,
        &frame, &frame_size    );
    
    if(!isack) {
        linkstate->send_expectedackdelay = (                (       
                        (frame_size+sizeof(cactusnet_Frame_Header))                    *   (8e6 / linkinfo[linkstate->linkid].bandwidth)                )
            +   2*linkinfo[linkstate->linkid].propagationdelay        );    }
    
    state->options.physicalsendcb(linkstate->linkid, frame, &frame_size);
    
    return 0;}

int cactusnet_data_rawreceive(
    cactusnet_NodeState *state,
    cactusnet_LinkState *linkstate,
    char *frame, size_t frame_size
  ) {
    bool errordetected;
    cactusnet_FrameType type;
    char *packet; size_t packet_size;
    uint32_t checksum_received;
    uint32_t checksum_computed;
    cactusnet_data_parseframe(
        frame, frame_size,
        &type,
        &packet, &packet_size,
        &errordetected,
        &checksum_received,
        &checksum_computed
    );
    
    linkstate->receive_prevtime = nodeinfo.time_in_usec;
    if(errordetected) {        dbg("Corrupt frame (%x != %x)", checksum_received, checksum_computed);
        // send negative acknowledgement?
        // set receive status to "wait"; source will resend once timeout expires
        linkstate->receive_status = cactusnet_LinkState_ReceiveStatus_ERROR;
    } else {
        linkstate->receive_status = cactusnet_LinkState_ReceiveStatus_OK;
        
        if(type == cactusnet_FrameType_ACK0) {
            dbg("Received ACK0 (ex%d) from l%d", 1-linkstate->send_seq, linkstate->linkid);
            // Partner has received sent frame 1, we can send frame 0
            if(!linkstate->send_ackreceived && linkstate->send_seq == 1) {
                linkstate->send_ackreceived = true;
                char *packet; size_t packet_size;
                dbg("popping from %d items %p %p", queue_nitems(linkstate->send_buffer), linkstate->send_buffer, &packet_size);
                packet = queue_remove(linkstate->send_buffer, &packet_size);
                if(packet) {
                    //free(packet);
                } else {                    CHECK(-1);                }
            } else {
                // wrong ack, ignore it
                RESCHECK(-1);
            }
        } else if(type == cactusnet_FrameType_ACK1) {
            dbg("Received ACK1 (ex%d) from l%d", 1-linkstate->send_seq, linkstate->linkid);
            // Partner has received sent frame 0, we can send frame 1
            if(!linkstate->send_ackreceived && linkstate->send_seq == 0) {
                linkstate->send_ackreceived = true;
                char *packet; size_t packet_size;
                packet = queue_remove(linkstate->send_buffer, &packet_size);
                free(packet);
            } else {
                RESCHECK(-1);
            }
        } else {
            dbg("Received DATA%d (ex%d) from l%d", type, 1-linkstate->receive_seq, linkstate->linkid);
            bool receive_seq = (
                    type == cactusnet_FrameType_DATA0 ? 0
                :   type == cactusnet_FrameType_DATA1 ? 1
                :   -1
            );
            VALCHECK(receive_seq != -1);
            if(receive_seq != linkstate->receive_seq) {                linkstate->receive_seq = 1-linkstate->receive_seq;
                cactusnet_net_handlepacket(
                    state,
                    linkstate,
                    packet, packet_size
                );
                cactusnet_data_rawsend(                    state,
                    linkstate,
                    true, false,
                    NULL, 0                );
            } else {                // Duplicate            }
        }
        
        if(linkstate->send_ackreceived && queue_nitems(linkstate->send_buffer) > 0) {            dbg("Processing queued frames");            char *packet; size_t packet_size;
            packet = queue_peek(linkstate->send_buffer, &packet_size);
            dbg("PTS %p", packet);            cactusnet_data_rawsend(                state,
                linkstate,
                false, false,
                packet, packet_size            );        }
    }
    
    return 0;
}

int cactusnet_data_makeframe(
    cactusnet_FrameType type,
    char *packet, size_t packet_size, // contained data from higher layers
    char **p_frame, size_t *p_frame_size // frame data to be sent over wire
  ) {    if(packet == NULL) {        packet_size = 0;
        packet = "";    }
    *p_frame_size = sizeof(cactusnet_Frame_Header)+packet_size;
    dbg("FS = %d", *p_frame_size);
    char *frame_it = malloc(*p_frame_size); 
    *p_frame = frame_it;
    
    cactusnet_Frame_Header *header = (cactusnet_Frame_Header*)(frame_it);
    memcpy(&header->preamble, cactusnet_FRAME_PREAMBLE, sizeof(header->preamble));
    header->type = type;
    
    frame_it += sizeof(cactusnet_Frame_Header);
    dbg("PTS %p", packet);
    memcpy(frame_it, packet, packet_size);
    
    header->checksum = 0;
    header->checksum = CNET_crc32((unsigned char*)(*p_frame), *p_frame_size);
    dbg("SEND %x S%d", header->checksum, *p_frame_size);
    if(((cactusnet_Packet_Header*)packet)->type == cactusnet_PacketType_NET_UPDATE)
        dbg("SENDa %d", ((cactusnet_Packet_NetUpdate*)packet)->routetable_count);
    
    return 0;
}
int cactusnet_data_parseframe(
    char *frame, size_t frame_size,
    cactusnet_FrameType *type,
    char **p_packet, size_t *p_packet_size,
    bool *errordetected,
    uint32_t *csreceived,
    uint32_t *cscomputed
  ) {
    *errordetected = false;
    cactusnet_Frame_Header *header = (cactusnet_Frame_Header*)frame;
    if(memcmp(header->preamble, cactusnet_FRAME_PREAMBLE, sizeof(header->preamble)) != 0) {
        *errordetected = true;
    }
    *type = header->type;
    uint32_t receivedchecksum = header->checksum;
    *p_packet = frame+sizeof(cactusnet_Frame_Header);
    *p_packet_size = frame_size-sizeof(cactusnet_Frame_Header);
    
    header->checksum = 0;
    uint32_t computedchecksum = CNET_crc32((unsigned char*)frame, frame_size);
    if(computedchecksum != receivedchecksum) {
        *errordetected = true;
    }
    if(csreceived != NULL) *csreceived = receivedchecksum;
    if(cscomputed != NULL) *cscomputed = computedchecksum;
    dbg("RECV %x S%d", receivedchecksum, frame_size);
    if(((cactusnet_Packet_Header*)(*p_packet))->type == cactusnet_PacketType_NET_UPDATE)
        dbg("RECVa %d", ((cactusnet_Packet_NetUpdate*)(*p_packet))->routetable_count);
    
    return 0;
}
