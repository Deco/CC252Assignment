
#define _GNU_SOURCE // for asprintf
#include <cnet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/param.h>
#include <string.h>


// CNet does not handle the typical C source & header structure very well
// We have to behave as if there is one source file and one header file
// See the bottom of this file for where the other source files are included
#include "cactusnet.h"
#include "cactusnet_datalayer.h"
#include "cactusnet_netlayer.h"

CnetTime cnet_boot_Delay_usec = 500000;
const size_t BUFFERINITIALSIZE = 1e4;

cactusnet_NodeState *state;
bool timer_scheduled = 0;
CnetTime timer_eta = 0;
bool ticktoggle = 0;

void cnet_process(cactusnet_NodeState *state);

static EVENT_HANDLER(cnet_applicationready)
{
    void *msg;
    size_t msg_size = BUFFERINITIALSIZE;
    CnetAddr msg_dest;
    msg = malloc(msg_size);
    MEMCHECK(msg);
    
    int res = CNET_read_application(&msg_dest, msg, &msg_size);
    if(res == -1 && cnet_errno == ER_BADSIZE) {
        msg = realloc(msg, msg_size);
        MEMCHECK(msg);
        CHECK(CNET_read_application(&msg_dest, msg, &msg_size));
    } else {
        CHECK(res);
    }
    
    RESCHECK(cactusnet_transport_send(state, msg_dest, msg, msg_size));
    cnet_process(state);
    
    free(msg);
}

static EVENT_HANDLER(cnet_physicalready)
{
    int linkid;
    
    dbg("");
    char *frame;
    size_t frame_size = BUFFERINITIALSIZE;
    dbg("");
    frame = calloc(frame_size, sizeof(char));
    dbg("");
    MEMCHECK(frame);
    dbg("%p %d", frame, frame_size);
    int res = CNET_read_physical(&linkid, (void*)frame, &frame_size);
    dbg("");
    if(res == -1 && cnet_errno == ER_BADSIZE) {
        frame = realloc(frame, frame_size);
    dbg("");
        MEMCHECK(frame);
    dbg("");
        CHECK(CNET_read_physical(&linkid, frame, &frame_size));
    dbg("");
    } else {
        CHECK(res);
    }
    
    dbg("");
    RESCHECK(cactusnet_physical_receive(state, linkid, frame, frame_size));
    dbg("");
    cnet_process(state);
    dbg("");
    
    //free(frame);
}

static EVENT_HANDLER(cnet_linkstatechange)
{
    int linkid = (int)data;
    if(linkinfo[linkid].linkup) {
        dbg("Link %d is up", linkid);
        RESCHECK(cactusnet_onlinkup(state, linkid));
    } else {
        dbg("Link %d is down", linkid);
        RESCHECK(cactusnet_onlinkdown(state, linkid));
    }
    cnet_process(state);
}

CnetEvent cnet_tick_TimerEvent = EV_TIMER2;
static EVENT_HANDLER(cnet_tick)
{
    cnet_process(state);
}

int cnet_downtophysical(int link, const void *data, size_t *data_size) {
    CHECK(CNET_write_physical(link, (void*)data, data_size));
    return 0;
}
int cnet_uptoapplication(CnetAddr srcaddr, const void* data, size_t *data_size) {    CHECK(CNET_write_application((void*)data, data_size));
    return 0;}

void draw_frame(CnetEvent ev, CnetTimerID timer, CnetData data);

CnetEvent cnet_boot_TimerEvent = EV_TIMER1;
static EVENT_HANDLER(cnet_boot)
{
    CNET_set_LED(0, "green");
    cactusnet_NodeOptions options;
    cactusnet_NodeOptions_init(&options);
    options.physicalsendcb = &cnet_downtophysical;
    options.transportreceivecb = &cnet_uptoapplication; 
    options.nodecount = 255; // in case we want to test 255 nodes!
    options.nlinks = nodeinfo.nlinks;
    RESCHECK(cactusnet_init(
        state,
        options,
        nodeinfo.nodenumber,
        nodeinfo.address
    ));
    dbg("Ready @%d", nodeinfo.address);
    
    for(int linkid = 1; linkid <= nodeinfo.nlinks; linkid++) {
        RESCHECK(cactusnet_onlinkup(state, linkid));
        dbg("Link %d is up (init)", linkid);
    }
    cnet_process(state);
    CNET_enable_application(ALLNODES);
}

void reboot_node(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    state = malloc(sizeof(cactusnet_NodeState));
    
    CNET_set_LED(0, "red");
    
    CHECK(CNET_set_handler(EV_APPLICATIONREADY  , cnet_applicationready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY     , cnet_physicalready   , 0));
    CHECK(CNET_set_handler(EV_LINKSTATE         , cnet_linkstatechange , 0));
    CHECK(CNET_set_handler(cnet_tick_TimerEvent , cnet_tick            , 0));
    CHECK(CNET_set_handler(cnet_boot_TimerEvent , cnet_boot            , 0));
    CHECK(CNET_set_handler( EV_DRAWFRAME,        draw_frame, 0));
    
    CNET_start_timer(cnet_boot_TimerEvent, cnet_boot_Delay_usec, 0);
}

void cnet_process(cactusnet_NodeState *state)
{
    bool next_scheduled = 0;
    CnetTime next_eta = 0;
    RESCHECK(cactusnet_process(state, nodeinfo.time_in_usec, &next_scheduled, &next_eta));
    
    if(next_scheduled) {
        if(timer_scheduled) {
            CNET_stop_timer(cnet_tick_TimerEvent);
            timer_scheduled = false;
        }
        timer_eta = next_eta;
        CNET_start_timer(cnet_tick_TimerEvent, timer_eta, 0);
        timer_scheduled = true;
        if(ticktoggle) {
            CNET_set_LED(1, "#0000FF");
        } else {
            CNET_set_LED(1, "#000088");
        }
        ticktoggle = !ticktoggle;
    } else if(timer_scheduled) {
        CNET_stop_timer(cnet_tick_TimerEvent);
        CNET_set_LED(1, CNET_LED_OFF);
        timer_scheduled = true;
    }
}

#include "cactusnet_datalayer.h"

void draw_frame(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    CnetDrawFrame *df  = (CnetDrawFrame*)data;
    cactusnet_Frame_Header *f = (cactusnet_Frame_Header*)df->frame;
    
    switch(f->type) {
        case cactusnet_FrameType_ACK0:
            df->nfields    = 1;
            df->colours[0] = "red";
            df->pixels[0]  = 10;
            sprintf(df->text, "ack=0");
            break;
        case cactusnet_FrameType_ACK1:
            df->nfields    = 1;
            df->colours[0] = "purple";
            df->pixels[0]  = 10;
            sprintf(df->text, "ack=1");
            break;
        case cactusnet_FrameType_DATA0:
            df->nfields    = 2;
            df->colours[0] = "red";
            df->pixels[0]  = 10;
            df->colours[1] = "green";
            df->pixels[1]  = 30;
            sprintf(df->text, "data=0");
            break;
        case cactusnet_FrameType_DATA1:
            df->nfields    = 2;
            df->colours[0] = "purple";
            df->pixels[0]  = 10;
            df->colours[1] = "green";
            df->pixels[1]  = 30;
            sprintf(df->text, "data=1");
            break;
        default:            break;
    }
}

#include "cactusnet.c"
#include "cactusnet_datalayer.c"
#include "cactusnet_netlayer.c"


