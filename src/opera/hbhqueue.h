#ifndef HBHQUEUE_H
#define HBHQUEUE_H

#include "datacenter/dynexp_topology.h"
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include <list>
#include <map>

class HbHQueue : public Queue {
 public:
    HbHQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger, int tor, int port, DynExpTopology *top,
        unsigned D, unsigned P);
    void receivePacket(Packet &pkt);
    void beginService();
    void completeService();
    void doNextEvent();
    void reportLoss() {}
    void packetWasDropped(Packet& pkt); //update invariants from fabric if packet was dropped on the way
    void poll(); //receveing queue checks if sending queues have something to send, true if found
    void pollme(Queue *me);
    int getNextPort(Packet &pkt);
    int getNextNextPort(Packet &pkt);
    Queue* getNextQueue(Packet &pkt);
    Queue* getNextQueue(int port);
    virtual void reportMaxqueuesize();
    inline bool isTherePkt(int port) { return _port_to_queued[port] > 0; }
    inline bool is_servicing() { return _servicing; };
    inline unsigned get_pkts_per_flow(uint64_t flow_id) { return _flowid_to_queued[flow_id]; }
    inline unsigned get_pkts_per_dst(int dst) { return _dst_to_queued[dst]; }
    bool extractNext(int port, map<uint64_t, unsigned> &flowid_to_queued, map<int, unsigned> &dst_to_queued, map<int, unsigned> &port_to_queued); 
    virtual void setupNICPkt(Packet *pkt) {}
 protected:
    bool is_queue_connected(Queue* q);
    void enqueuePacket(Packet &pkt);
    map<int, unsigned> _port_to_queued;
    map<uint64_t, unsigned> _flowid_to_queued;
    map<int, unsigned> _dst_to_queued;
    map<int, unsigned> _hops_to_queued;
    bool _servicing;
    bool _is_NIC;
    bool _is_timer_set;
    bool _is_downlink;
    bool _sorted_enqueue;
    unsigned _D; //max packets per flow in queue
    unsigned _P; //max packets per dest port in queue
    Packet* _next_tx_pkt;
    list<Queue*> _queues_waiting;
};

class HbHNICQueue : public HbHQueue {
 public:
    HbHNICQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger, int node, DynExpTopology *top,
        unsigned D, unsigned P);
    void receivePacket(Packet &pkt);
    void completeService();
    virtual void setupNICPkt(Packet* pkt);
 protected:
    int _node;
};

#endif
