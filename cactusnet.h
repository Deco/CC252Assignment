
#include <cnet.h>
#include <cnetsupport.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef CACTUSNET_H
#define CACTUSNET_H

#define RESCHECK(c) { cnet_errno = 0; CHECK(c); }
#define VALCHECK(c) { cnet_errno = 0; CHECK((c) ? 0 : -1); }
#define MEMCHECK(c) { cnet_errno = 0; CHECK(((int)(c)) == 0 ? -1 : 0); }
#define dbg(...) _dbg(__LINE__, __VA_ARGS__)

// CNet does not handle stdio.h properly, and these functions are needed.
extern int asprintf (char **__restrict __ptr,
		     __const char *__restrict __fmt, ...)
     __THROW __attribute__ ((__format__ (__printf__, 2, 3)));
extern int vasprintf (char **__restrict __ptr, __const char *__restrict __f,
		      _G_va_list __arg)
     __THROW __attribute__ ((__format__ (__printf__, 2, 0)));
//

typedef int (*cactusnet_PhysicalSendCallback)(int link, const void *data, size_t *data_size);
typedef int (*cactusnet_TransportReceiveCallback)(CnetAddr srcaddr, const void *data, size_t *data_size);
typedef struct cactusnet_NodeOptions_s {
    cactusnet_PhysicalSendCallback physicalsendcb;
    cactusnet_TransportReceiveCallback transportreceivecb;
    int nodecount;
    int nlinks;
} cactusnet_NodeOptions;
void cactusnet_NodeOptions_init(cactusnet_NodeOptions *obj);

typedef enum cactusnet_NodeState_Flag_e {
    cactusnet_NodeState_Flag_NONE = 0,
    cactusnet_NodeState_Flag_
} cactusnet_NodeState_Flag;


typedef enum cactusnet_LinkState_Status_e {
    cactusnet_LinkState_Status_DISCONNECTED = 0,
    cactusnet_LinkState_Status_HANDSHAKING     ,
    cactusnet_LinkState_Status_CONNECTED       ,
    cactusnet_LinkState_Status_TIMEOUT         ,
    cactusnet_LinkState_Status_e_max
} cactusnet_LinkState_Status;

typedef struct cactusnet_RouteInfo_s {
    CnetAddr addr;
    long distance;
    int locallinkid;
} cactusnet_RouteInfo;
typedef HASHTABLE cactusnet_RouteInfoTable;

typedef VECTOR cactusnet_NodeAddrVector;

typedef enum cactusnet_LinkState_ReceiveStatus_e {
    cactusnet_LinkState_ReceiveStatus_OK    = 0, // Everything as expected
    cactusnet_LinkState_ReceiveStatus_ERROR    , // Error, wait for resend
    cactusnet_LinkState_ReceiveStatus_e_max
} cactusnet_LinkState_ReceiveStatus;

typedef enum cactusnet_LinkState_SendStatus_e {
    cactusnet_LinkState_SendStatus_IDLE = 0, // Nothing sending
    cactusnet_LinkState_SendStatus_WAIT    , // Waiting for acknowledgement
    cactusnet_LinkState_SendStatus_e_max
} cactusnet_LinkState_SendStatus;

typedef QUEUE cactusnet_DataQueueBuffer;

typedef struct cactusnet_LinkState_s {
    int linkid;
    cactusnet_LinkState_Status status;
    CnetAddr partner_addr;
    //cactusnet_RouteInfoTable routetable; // vector of cactusnet_RouteInfo*
    
    cactusnet_LinkState_ReceiveStatus receive_status;
    bool receive_seq; /* sequence number of frame just received */
    bool receive_acksent; /* have we sent the ack for received frame yet? */
    //int receive_attempts;
    CnetTime receive_prevtime;
    //cactusnet_DataQueueBuffer receive_buffer;
    
    cactusnet_LinkState_SendStatus sendstatus;
    bool send_seq; /* sequence number of frame just sent */
    bool send_ackreceived; /* has the previous frame been acknowledged? */
    //int send_attempts;
    cactusnet_DataQueueBuffer send_buffer;
    CnetTime send_prevtime;
    CnetTime send_expectedackdelay;
} cactusnet_LinkState;
typedef VECTOR cactusnet_LinkStateVector;

typedef struct cactusnet_NodeState_s {
    cactusnet_NodeOptions options;
    int nodeid;
    CnetAddr addr;
    cactusnet_LinkState **linkarray;
    cactusnet_RouteInfoTable routetable;
    cactusnet_NodeAddrVector knownnodesvector; // for iterating routetable
    int routetable_size;
} cactusnet_NodeState;

int cactusnet_init(
    cactusnet_NodeState *state,
    cactusnet_NodeOptions options,
    int nodeid,
    CnetAddr addr
);
int cactusnet_process(
    cactusnet_NodeState *state,
    CnetTime time,
    bool *next_scheduled,
    CnetTime *next_eta
);
int cactusnet_onlinkup(
    cactusnet_NodeState *state,
    int linkid
);
int cactusnet_onlinkdown(
    cactusnet_NodeState *state,
    int linkid
);
int cactusnet_physical_receive(
    cactusnet_NodeState *state,
    int linkid,
    void *frame, size_t frame_size
);
int cactusnet_physical_send(    cactusnet_NodeState *state,
    int linkid,
    void *frame, size_t frame_size);
int cactusnet_transport_send(
    cactusnet_NodeState *state,
    CnetAddr destaddr,
    void *msg, size_t msg_size
);
int cactusnet_transport_receive(    cactusnet_NodeState *state,
    CnetAddr srcaddr,
    void *msg, size_t msg_size);

char *cactusnet_getnodekey(CnetAddr addr);

void _dbg(int line, char *fmt, ...);

#endif

