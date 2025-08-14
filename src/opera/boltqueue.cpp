// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "boltqueue.h"
#include <math.h>
#include "datacenter/dynexp_topology.h"
#include "ecn.h"
#include "eventlist.h"
#include "tcp.h"
#include "dctcp.h"
#include "queue_lossless.h"
#include "tcppacket.h"
#include <iostream>

//#define UNSCHED_BOUND

BoltQueue::BoltQueue(linkspeed_bps bitrate, mem_b maxsize, 
                     EventList& eventlist, QueueLogger* logger,
                     int tor, int port, DynExpTopology *top)
: Queue(bitrate,maxsize,eventlist,logger,tor,port,top)
{
  _CCthresh = 1*MTU_SIZE;
  _pru_token = 0;
  _sm_token = 0;
  _maxsize_unsched = 10*MTU_SIZE;
  _last_sm_t = 0;
#ifdef PRIO_QUEUE
  _servicing = Q_NONE;
  _queuesize[Q_LO] = 0;
  _queuesize[Q_HI] = 0;
#endif
}

void
BoltQueue::updateSupply(Packet &pkt) {
  double interarrival_t = timeAsSec(eventlist().now()-_last_sm_t);
  _last_sm_t = eventlist().now();
  int64_t bw = 1E11/8; //bytes per second
  int64_t supply = bw * interarrival_t;
  int64_t demand = pkt.size();
  _sm_token += supply-demand;
  _sm_token = min(_sm_token, MTU_SIZE);
  //cout << "SMTOKEN " << _sm_token << " " << supply << " " << demand << " " << interarrival_t <<endl;
}

void
BoltQueue::receivePacket(Packet & pkt)
{
  queue_priority_t prio; 
#ifdef PRIO_QUEUE
  switch(pkt.type()) {
    case TCPACK:
      prio = Q_HI;
      break;
    default:
      prio = Q_LO; 
  }
  assert(prio == Q_HI || prio == Q_LO);
#endif
/*
  cout << nodename() << " receivePacket " << pkt.size() << " " << pkt.flow_id() << " " << queuesize() << " " << pkt.get_slice_sent() << " " << _top->time_to_slice(eventlist().now()) << 
    " " << eventlist().now() << endl;
*/
 
  if (queuesize()+pkt.size() > _maxsize) {
    /* if the packet doesn't fit in the queue, drop it */
    if(pkt.type() == TCP){
      TcpPacket *tcppkt = (TcpPacket*)&pkt;
      tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
      if(tcppkt->flow_id() == 75184) cout << "DROPPED HERE (queue) seqno " << tcppkt->seqno() << endl;
    }
    cout << nodename() << " DROPPED " << queuesize() << " " << _top->time_to_slice(eventlist().now()) << endl;
    pkt.free();
    _top->inc_losses();
    _num_drops++;
    return;
  }

  if(pkt.type() == TCP) {
    updateSupply(pkt);
  }

  //SRC packets only generated for data packets, one time per packet at most
  if(pkt.type() == TCP && !pkt.early_fb() && queuesize() > _CCthresh) {
    //cout << "BoltQueue " << _tor << " " << _port << " SRC triggered\n";
    pktINT pkt_int(queuesize(), _txbytes, eventlist().now());
    pkt.push_int("bolt", pkt_int);
    sendEarlyFeedback(pkt);
    pkt.set_early_fb();
  //increase pru_tokens if packet is last in flow AND flow is not first BDP
  } else if(pkt.type() == TCP && ((TcpPacket*)&pkt)->last() && !((TcpPacket*)&pkt)->first()) {
    _pru_token++;
  } else if (pkt.type() == TCP && ((TcpPacket*)&pkt)->bolt_inc()) {
    if (_pru_token > 0) {
      _pru_token--;
    } else if (_sm_token >= MTU_SIZE) {
      _sm_token -= MTU_SIZE;
    } else {
      ((TcpPacket*)&pkt)->set_bolt_inc(false);
    }
  }

  //AFTER sending potential SRC, drop packets if first BDP above unsched threshold
#ifdef UNSCHED_BOUND
  if(pkt.type() == TCP) {
      TcpPacket *tcppkt = (TcpPacket*)&pkt;
      if (queuesize()+pkt.size() > _maxsize_unsched && tcppkt->first() && tcppkt->seqno() != 1) {
        //cout << nodename() << " DROPPED UNSCHED flowid " << tcppkt->flow_id() << " seqno " << tcppkt->seqno() << " " << queuesize() << " " << _top->time_to_slice(eventlist().now()) << endl;
        tcppkt->get_tcpsrc()->signalLoss(tcppkt->seqno());
        pkt.free();
        return;
      }
  }
#endif

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
#ifdef PRIO_QUEUE
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

  //record queuesize per_queuesize slice
  int slice = _top->time_to_superslice(eventlist().now());
  if (queuesize() > _max_recorded_size_slice[slice]) {
    _max_recorded_size_slice[slice] = queuesize();
  }
  if (queuesize() > _max_recorded_size) {
    _max_recorded_size = queuesize();
  }

  if (queueWasEmpty) {
    /* schedule the dequeue event */
#ifdef PRIO_QUEUE
    assert(_enqueued[prio].size() == 1);
#else
    assert(_enqueued.size() == 1);
#endif
    beginService();
  }

}

void BoltQueue::beginService() {
  /* schedule the next dequeue event */
#ifdef PRIO_QUEUE
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
BoltQueue::completeService()
{
  /* dequeue the packet */
#ifdef PRIO_QUEUE
  assert(!_enqueued[_servicing].empty());
  Packet* pkt = _enqueued[_servicing].back();
  _enqueued[_servicing].pop_back();
  assert(_queuesize[_servicing] >= pkt->size());
  _queuesize[_servicing] -= pkt->size();
#else
  assert(!_enqueued.empty());
  Packet* pkt = _enqueued.back();
  _enqueued.pop_back();
  _queuesize -= pkt->size();
#endif
  
  sendFromQueue(pkt);

#ifdef PRIO_QUEUE
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
BoltQueue::queuesize() {
#ifdef PRIO_QUEUE
  return _queuesize[Q_LO] + _queuesize[Q_HI]; 
#else
  return _queuesize;
#endif

}
