// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef CRED_QUEUE_H
#define CRED_QUEUE_H
#include "queue.h"
/*
 * A simple ECN queue that marks on dequeue as soon as the packet occupancy exceeds the set threshold. 
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class CreditQueue : public Queue {
 public:
    CreditQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger);
    void receivePacket(Packet & pkt);
    void beginService();
    void completeService();
    void doNextEvent();
    void reportLoss();
 protected:
    enum pkt_type {NONE, DATA, CRED};
    pkt_type _tx_next;
    void updateAvailCredit();
    void scheduleCredit();
    simtime_picosec cred_tx_delta();
    bool credit_ready();
    mem_b _maxsize_cred;
    mem_b _queuesize_cred;
    mem_b _data_size;
    int _avail_cred, _max_avail_cred;
    simtime_picosec _last_cred_t;
    simtime_picosec _last_cred_tx_t;
    simtime_picosec _next_sched_tx;
    bool _cred_tx_pending;
    list<Packet*> _enqueued_cred;
    uint64_t _tot_creds;
    uint64_t _drop_creds;
};

class NICCreditQueue : public CreditQueue {
 public:
    NICCreditQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger);
    void completeService();

};

#endif
