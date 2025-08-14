// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "ecnqueue.h"
#include <math.h>
#include "datacenter/dynexp_topology.h"
#include "ecn.h"
#include "tcp.h"
#include "dctcp.h"
#include "queue_lossless.h"
#include "tcppacket.h"
#include <iostream>

ECNQueue::ECNQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger, mem_b  K,
             int tor, int port, DynExpTopology *top)
    : Queue(bitrate,maxsize,eventlist,logger,tor,port,top), 
      _K(K)
{
    _state_send = LosslessQueue::READY;
#ifdef PRIO_ECNQUEUE
    _servicing = Q_NONE;
#endif
}


void
ECNQueue::receivePacket(Packet & pkt)
{
    queue_priority_t prio; 
#ifdef PRIO_ECNQUEUE
    switch(pkt.type()) {
        case TCPACK:
        //case SAMPLE:
            if(pkt.early_fb()
                prio = Q_HI;
            else
                prio = Q_LO;
            break;
        default:
            prio = Q_LO; 
    }
    assert(prio == Q_HI || prio == Q_LO);
#endif
    //cout << nodename() << " receivePacket " << pkt.flow_id() << " queuesize " << _queuesize << endl;
    if (queuesize()+pkt.size() > _maxsize) {
        /* if the packet doesn't fit in the queue, drop it */
        /*
           if (_logger) 
           _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
           pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
           */
        if(pkt.type() == TCP){
            TcpPacket *tcppkt = (TcpPacket*)&pkt;
            tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
            cout << "DROPPED\n";
        }
        pkt.free();
        _top->inc_losses();
        _num_drops++;
        return;
    }

    //TEST: early feedback one-way back delay at random hop
    /*
    if(pkt.type() == TCP && pkt.get_crthop() == pkt.get_earlyhop() && !pkt.early_fb()) {
        sendEarlyFeedback(pkt);
        pkt.set_early_fb();
    }
    */

/*
    if (queuesize() > _K && pkt.type() == TCP && !pkt.early_fb()){
        //TEST early fb in response to congestion
        sendEarlyFeedback(pkt);
        pkt.set_early_fb();
        //better to mark on dequeue, more accurate
        //pkt.set_flags(pkt.flags() | ECN_CE);
    }
*/

    /* enqueue the packet */
    updatePktIn(pkt.flow_id());
#ifdef PRIO_ECNQUEUE
    bool queueWasEmpty = _servicing == Q_NONE;
    _enqueued[prio].push_front(&pkt);
    _queuesize[prio] += pkt.size();
    pkt.inc_queueing(_queuesize[prio]);
    pkt.set_last_queueing(_queuesize[prio]);
#else
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();
    pkt.inc_queueing(_queuesize);
    pkt.set_last_queueing(_queuesize);
#endif
/*
    if(_top->is_last_hop(_port)) {
        cout << "CORE RATIO " << (double)_queuesize/pkt.get_queueing() <<  " " << _queuesize << " " << pkt.get_queueing() << endl;
    }
*/

    //record queuesize per slice
    int slice = _top->time_to_superslice(eventlist().now());
    if (queuesize() > _max_recorded_size_slice[slice]) {
        _max_recorded_size_slice[slice] = queuesize();
    }
    if (queuesize() > _max_recorded_size) {
        _max_recorded_size = queuesize();
    }

    if (queueWasEmpty) {
	/* schedule the dequeue event */
#ifdef PRIO_ECNQUEUE
	assert(_enqueued[prio].size() == 1);
#else
	assert(_enqueued.size() == 1);
#endif
	beginService();
    }
    
}

void ECNQueue::beginService() {
    /* schedule the next dequeue event */
#ifdef PRIO_ECNQUEUE
    assert(!_enqueued[Q_LO].empty() || !_enqueued[Q_HI].empty());
    if(!_enqueued[Q_HI].empty()) {
        assert(!_enqueued[Q_HI].empty());
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[Q_HI].back()));
        _servicing = Q_HI;
    } else {
        assert(!_enqueued[Q_LO].empty());
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[Q_LO].back()));
        _servicing = Q_LO;
    }
#else
    assert(!_enqueued.empty());
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
#endif
}

void
ECNQueue::completeService()
{
	/* dequeue the packet */
#ifdef PRIO_ECNQUEUE
    assert(!_enqueued[_servicing].empty());
    Packet* pkt = _enqueued[_servicing].back();
    _enqueued[_servicing].pop_back();
    //mark on deque
    if (queuesize() > _K){
	  pkt->set_flags(pkt->flags() | ECN_CE);
    }
    _queuesize[_servicing] -= pkt->size();
#else
    assert(!_enqueued.empty());
    Packet* pkt = _enqueued.back();
    _enqueued.pop_back();
    if (queuesize() > _K){
	  pkt->set_flags(pkt->flags() | ECN_CE);
    }
    _queuesize -= pkt->size();
#endif

    sendFromQueue(pkt);

#ifdef PRIO_ECNQUEUE
    _servicing = Q_NONE;
    if (!_enqueued[Q_HI].empty() || !_enqueued[Q_LO].empty()) {
	/* schedule the next dequeue event */
	beginService();
    }
#else
    if (!_enqueued.empty()) {
	/* schedule the next dequeue event */
	beginService();
    }
#endif
}

mem_b
ECNQueue::queuesize() {
#ifdef PRIO_ECNQUEUE
    return _queuesize[Q_LO] + _queuesize[Q_HI]; 
#else
    return _queuesize;
#endif

}
