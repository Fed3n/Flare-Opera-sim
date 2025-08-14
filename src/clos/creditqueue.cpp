// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "creditqueue.h"
#include "config.h"
#include "tcp.h"
#include "tcppacket.h"
#include <math.h>
#include <iostream>
#include <type_traits>

#define NO_PENDING_TX (simtime_picosec)(-1); //unsigned so max unit

CreditQueue::CreditQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger)
    : Queue(bitrate,maxsize,eventlist,logger) 
{
  _maxsize_cred = 64*8;
  _max_avail_cred = 2;
  _avail_cred = 1;
  _queuesize_cred = 0;
  _last_cred_t = 0;
  _last_cred_tx_t = 0;
  _tot_creds = 0;
  _drop_creds = 0;
  _next_sched_tx = NO_PENDING_TX;
  _tx_next = NONE;
  _cred_tx_pending = false;
  _data_size = 1575;
}

simtime_picosec
CreditQueue::cred_tx_delta() {
  return eventlist().now() - _last_cred_t;
}

//for future self: if there is a weird bug with pacing, it may be here :) cause double events
bool
CreditQueue::credit_ready() {
  updateAvailCredit();
  //there is leftover credit available, can transmit
  if(_avail_cred > 0) {
    _avail_cred--;
    return true;
  } else {
    return false;
  }
}

void
CreditQueue::scheduleCredit() {
  simtime_picosec spacing = _ps_per_byte*_data_size;
  if(!_cred_tx_pending) {
    assert(spacing > cred_tx_delta());
    eventlist().sourceIsPendingRel(*this, spacing - cred_tx_delta());
    _cred_tx_pending = true;
  }
}

void
CreditQueue::updateAvailCredit() {
  simtime_picosec spacing = _ps_per_byte*_data_size;
  int new_cred = cred_tx_delta()/spacing;
  _avail_cred += new_cred;
  _avail_cred = min(_avail_cred, _max_avail_cred);
  //cout << nodename() << " updateAvailCredit new_cred " << new_cred << " avail " << _avail_cred << " elapsed " << eventlist().now()-_last_cred_t << endl;
  _last_cred_t += new_cred * spacing;
}

void
CreditQueue::receivePacket(Packet & pkt)
{
  updateAvailCredit();
  bool queueWasEmpty = _enqueued.empty() && _enqueued_cred.empty();
  //cout << nodename() << " receivePacket " << pkt.size() << " " << this << endl;
  if(pkt.type() == XPCREDIT) {
    _tot_creds++;
    //cout << "xpcredit\n";
    if (_queuesize_cred+pkt.size() > _maxsize_cred) {
      /* if the credit doesn't fit in the queue, drop it */
      //cout << nodename() << " CREDIT DROPPED for " << pkt.flowid() << endl;
      pkt.free();
      _drop_creds++;
      return;
    }
    /* enqueue the packet */
    _enqueued_cred.push_front(&pkt);
    _queuesize_cred += pkt.size();
  } else {
    //cout << "xpdata\n";
    if (_queuesize+pkt.size() > _maxsize) {
      /* if the packet doesn't fit in the queue, drop it */
      if(pkt.type() == TCP){
        TcpPacket *tcppkt = (TcpPacket*)&pkt;
        tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
      }
      cout << nodename() << " DROPPED " << _queuesize << endl;
      pkt.free();
      _num_drops++;
      return;
    }
    /* enqueue the packet */
    if(queuesize() > _max_reported_size) {
      _max_reported_size = queuesize();
    }
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();
    pkt.inc_queueing(_queuesize);
    pkt.set_last_queueing(_queuesize);
    updatePktIn(pkt.flow_id());
    //cout << "enqueued xpdata\n";
  }

  if (queueWasEmpty) {
    /* schedule the dequeue event */
    if(pkt.type() == XPCREDIT){
      assert(_enqueued_cred.size() == 1 && _enqueued.size() == 0);
    } else {
      assert(_enqueued.size() == 1 && _enqueued_cred.size() == 0);
    }
    beginService();
  }
}

void
CreditQueue::beginService()
{
  /* schedule the next dequeue event */
  //cout << "data " << _enqueued.size() << " cred " << _enqueued_cred.size() << endl;
  assert(!(_enqueued.empty() && _enqueued_cred.empty()));
  assert(_tx_next == NONE);
  /*
  if(!_enqueued.empty()) {
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
    _next_sched_tx = eventlist().now()+drainTime(_enqueued.back());
    _tx_next = DATA;
  } else if (!_enqueued_cred.empty() && credit_ready()){
    _cred_tx_pending = false;
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_cred.back()));
    _next_sched_tx = eventlist().now()+drainTime(_enqueued_cred.back());
    _tx_next = CRED;
  }
  */
  if (!_enqueued_cred.empty() && credit_ready()){
    _cred_tx_pending = false;
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_cred.back()));
    _next_sched_tx = eventlist().now()+drainTime(_enqueued_cred.back());
    _tx_next = CRED;
  } else if (!_enqueued.empty()){
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
    _next_sched_tx = eventlist().now()+drainTime(_enqueued.back());
    _tx_next = DATA;
  } else {
    scheduleCredit();
  }
}

void
CreditQueue::completeService()
{
  /* dequeue the packet */
  Packet *pkt = NULL;
  if (_tx_next == CRED){
    //cout << "creditq completeService\n";
    assert(!_enqueued_cred.empty());
    pkt = _enqueued_cred.back();
    _enqueued_cred.pop_back();
    updatePktOut(pkt->flow_id());
    _queuesize_cred -= pkt->size();
    //assert(eventlist().now() - _last_cred_t >= 120000);
    //cout << nodename() << " completeService credit " << eventlist().now() << " dist " << eventlist().now()-_last_cred_tx_t << endl;
    _last_cred_tx_t = eventlist().now();
  } else {
    assert(!_enqueued.empty());
    pkt = _enqueued.back();
    _enqueued.pop_back();
    updatePktOut(pkt->flow_id());
    _queuesize -= pkt->size();
  }
  assert(pkt != NULL);
  /* tell the packet to move on to the next pipe */
  pkt->sendOn();
  _next_sched_tx = NO_PENDING_TX;
  _tx_next = NONE;
  /* schedule the next dequeue event */
  if(!(_enqueued.empty() && _enqueued_cred.empty())) {
    beginService();
  }
}

void
CreditQueue::doNextEvent() {
  if(eventlist().now() == _next_sched_tx) {
    //tx event
    completeService();
  } else if (_cred_tx_pending){
    //credit queue timer event
    assert(!_enqueued_cred.empty());
    if(_tx_next == NONE){
      beginService();
    }
  }
}

void
CreditQueue::reportLoss() {  
  cout << " " << _tot_creds << " " << _drop_creds;
}

NICCreditQueue::NICCreditQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger)
    : CreditQueue(bitrate,maxsize,eventlist,logger){} 

void
NICCreditQueue::completeService()
{
  //cout << nodename() << " completeService " << eventlist().now() << endl;
  /* dequeue the packet */
  Packet *pkt = NULL;
  if (_tx_next == CRED){
    //cout << "creditq completeService\n";
    assert(!_enqueued_cred.empty());
    pkt = _enqueued_cred.back();
    _enqueued_cred.pop_back();
    updatePktOut(pkt->flow_id());
    _queuesize_cred -= pkt->size();
    //cout << nodename() << " completeService credit " << eventlist().now() << " dist " << eventlist().now()-_last_cred_tx_t << endl;
    _last_cred_tx_t = eventlist().now();
  } else {
    assert(!_enqueued.empty());
    pkt = _enqueued.back();
    _enqueued.pop_back();
    updatePktOut(pkt->flow_id());
    _queuesize -= pkt->size();
  }
  assert(pkt != NULL);
  /* tell the packet to move on to the next pipe */
  pkt->set_fabricts(eventlist().now());
  pkt->set_queueing(0);
  pkt->sendOn();
  _next_sched_tx = NO_PENDING_TX;
  _tx_next = NONE;
  /* schedule the next dequeue event */
  if(!(_enqueued.empty() && _enqueued_cred.empty())) {
    beginService();
  }
}
