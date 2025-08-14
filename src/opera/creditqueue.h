// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef CRED_QUEUE_H
#define CRED_QUEUE_H
#include "datacenter/dynexp_topology.h"
#include "queue.h"
/*
 * A credit queue based on ExpressPass, Tidal adds probablistic shaping
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class CreditQueue : public Queue {
 public:
    CreditQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger, int tor, int port, DynExpTopology *top,
        mem_b credsize, mem_b shaping_thresh, mem_b aeolus_thresh, mem_b tent_thresh);
    void receivePacket(Packet & pkt);
    void beginService();
    void completeService();
    void doNextEvent();
    void reportLoss();
    virtual void reportMaxqueuesize();
 protected:
    enum pkt_type {NONE, DATA, CRED};
    pkt_type _tx_next;
    int _next_prio;
    void updateAvailCredit();
    void scheduleCredit();
    void handleCredit(Packet &pkt);
    simtime_picosec cred_tx_delta();
    bool credit_ready();
    int credit_prio(Packet &pkt);
    int next_cred();
    mem_b queuesize_cred(int prio); //queue within a certain prio
    mem_b queuesize_cred(); //full queue size
    mem_b _maxsize_cred;
    mem_b _maxsize_unsched;
    mem_b _shaping_thresh;
    vector<mem_b> _queuesize_cred;
    mem_b _data_size;
    int _max_tent_cred;
    int _avail_cred, _max_avail_cred;
    simtime_picosec _last_cred_t;
    simtime_picosec _last_cred_tx_t;
    simtime_picosec _next_sched_tx;
    simtime_picosec _cred_timeout;
    bool _cred_tx_pending;
    vector<list<Packet*>> _enqueued_cred;
    uint64_t _tot_creds;
    uint64_t _drop_creds;
    map<int, uint64_t> _hops_to_creds;
};

class NICCreditQueue : public CreditQueue {
 public:
    NICCreditQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger, DynExpTopology *top,
        mem_b credsize, mem_b shaping_thresh, mem_b aeolus_thresh, mem_b tent_thresh);
    void completeService();

};

#endif
