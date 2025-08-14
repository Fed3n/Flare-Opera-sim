#include <cstdint>
#include <cstdlib>
#include <math.h>
#include <iostream>
#include "config.h"
#include "datacenter/dynexp_topology.h"
#include "eventlist.h"
#include "hbh.h"
#include "hbhqueue.h"
#include "network.h"
#include "queue.h"
#include "hbhpacket.h"
#include "pipe.h"
#include <stdio.h>
#include <type_traits>
#include <tuple>
#include <list>

#define PERFECT_RECOVERY

static bool debug_flow(uint64_t flow_id) {
    return false;
    if(flow_id == 5196) return true;
    if(flow_id == 5836) return true;
    return false;
}

static uint64_t id_gen;

simtime_picosec HbHSrc::_min_rto = timeFromMs(0.2);

static uint64_t flow_id_gen;

HbHSrc::HbHSrc(EventList &eventlist, int flow_src, int flow_dst, DynExpTopology *top)
: EventSource(eventlist,"xpass"),  _top(top), _flow(NULL), _flow_src(flow_src), _flow_dst(flow_dst){
  _mss = Packet::data_packet_size();

  _base_rtt = timeInf;
  _acked_packets = 0;
  _packets_sent = 0;
  _new_packets_sent = 0;
  _rtx_packets_sent = 0;

  _flight_size = 0;

  _highest_sent = 0;
  _last_acked = 0;
  _finished = false;

  _sink = 0;

  _rtt = 0;
  _rto = timeFromMs(0.2);
  _max_NIC_pkts = 2;
  _max_inflight = Packet::data_packet_size()*20;
  _cwnd = 1 * Packet::data_packet_size();
  _init_cwnd = _cwnd;
  _mdev = 0;
  _drops = 0;
  _flow_size = ((uint64_t)1)<<63;
  _rtx_timeout_pending = false;
  _rtx_timeout = timeInf;
  _nodename = "hbhsrc" + to_string(_node_num);
  //_notifier = new HbHLinkUpNotifier(this);

  // debugging hack
  _log_me = false;
}

void 
HbHSrc::set_flowsize(uint64_t flow_size_in_bytes) {

  _flow_size = flow_size_in_bytes; //+1 for ackreq in first unscheduled BDP
  if (_flow_size < _mss)
    _pkt_size = _flow_size;
  else
    _pkt_size = _mss;
}

void HbHSrc::set_traffic_logger(TrafficLogger* pktlogger) {
  _flow.set_logger(pktlogger);
}

void HbHSrc::doNextEvent() {
  if (_rtx_timeout_pending) {
    _rtx_timeout_pending = false;
    retransmit_packet();
  } else {
    //cout << "Starting flow" << endl; // modification

    // debug:
    //if ( get_flow_src() == 0 && get_flow_dst() == 6) {
    //cout << "FST " << get_flow_src() << " " << get_flow_dst() << " " << get_flowsize() <<
    //    " " << timeAsMs(eventlist().now()) << " " << get_id() << endl;
    //}
    startflow();
  }
}

void HbHSrc::startflow(){
  _flow_id = flow_id_gen++;
  //cout << "startflow id " << _flow_id << " src " << _flow_src << " dst " << _flow_dst << " size " << _flow_size << endl;
  //intra-ToR traffic not implemented
  assert(_flow_src/6 != _flow_dst/6);
  _highest_sent = 0;
  _last_acked = 0;

  _packets_sent = 0;
  _rtx_timeout_pending = false;
  _rtx_timeout = timeInf;

  Queue *q = _top->get_queue_serv_tor(_flow_src);

  //TODO may want to add a total in_flight limiter
  while (q->get_pkts_per_flow(_flow_id) < _max_NIC_pkts && _highest_sent < _flow_size) {
    //send first burst of packets to NIC, always try to keep _max_NIC_pkts there
    send_packet();
  }
}

void
HbHSrc::setFinished() {
  if(!_finished) {
    _finished = true;
    cout << "UNFINISHED " << flow_id() << endl;
  }
}


void 
HbHSrc::connect(HbHSink& sink, simtime_picosec starttime) {

  _sink = &sink;
  _flow.id = id; // identify the packet flow with the XPASS source that generated it
  _flow._name = _name;
  _sink->connect(*this);

  set_start_time(starttime); // record the start time in _start_time

  //cout << "Flow hbhsrc" << flow_id() << " (name:" << _name << ") set to start at " << timeAsMs(starttime) << " ms" << endl;
  //cout << "FlowID " << id << " name " << _name << " SetStartms " << timeAsMS(starttime) << endl;
  //cout << "flowID " << id << " bytes " << get_flowsize() << " set_start_ms " << timeAsMs(starttime) << endl;
  eventlist().sourceIsPending(*this,starttime);

}

void 
HbHSrc::receivePacket(Packet& pkt) 
{
    assert(0);
    //shouldn't receive anything 
}

//when a packet is dropped in the network, immediately requeue it for retransmission
//this is an ideal form of recovery using global information
void 
HbHSrc::addToLost(HbHPacket *pkt){
    assert(_flight_size >= (pkt->size()-ACKSIZE));
    _flight_size -= pkt->size()-ACKSIZE;
#ifdef PERFECT_RECOVERY
    _rtx_queue.push_back(pkt);
    triggerSending();
#else
    //TODO RTO
    pkt->free();
#endif
}

void
HbHSrc::send_packet() {
    unsigned pkt_size = _highest_sent + _pkt_size > _flow_size ? _flow_size-_highest_sent : _pkt_size;
    assert(pkt_size > 0);
    assert(pkt_size <= 1436);
    bool last_packet = false;
    if (_highest_sent + pkt_size >= _flow_size) {
      last_packet = true;
    }
    HbHPacket* p = HbHPacket::newpkt(_top, _flow_src, _flow_dst, this, _sink, _highest_sent+1, pkt_size, false, last_packet);
    p->set_flow_id(_flow_id);

    _highest_sent += pkt_size;  //XX beware wrapping
    _packets_sent++;
    _new_packets_sent++;
    _flight_size += p->size()-ACKSIZE;

    PacketSink* sink = sendToNIC(p);
    HbHNICQueue *q = dynamic_cast<HbHNICQueue*>(sink);
    assert(q);
}

void
HbHSrc::retransmit_packet() {
    assert(!_rtx_queue.empty());
    Packet *p = _rtx_queue.front();
    _rtx_queue.pop_front();
    _packets_sent++;
    _rtx_packets_sent++;
    _flight_size += p->size()-ACKSIZE;

    PacketSink* sink = sendToNIC(p);
    HbHNICQueue *q = dynamic_cast<HbHNICQueue*>(sink);
    assert(q);
}

void
HbHSrc::triggerSending() {
  if(_finished) return;
  Queue *q = _top->get_queue_serv_tor(_flow_src);

  if(_flow_id == 522) {
    //cout << "TRIGGERSENDING pkts_per_flow " << q->get_pkts_per_flow(_flow_id) << " _flight_size " << _flight_size <<  endl;
  }
  //RTX packets, filled up with "global knowledge" of the network when something is lost
  while(q->get_pkts_per_flow(_flow_id) < _max_NIC_pkts && !_rtx_queue.empty() && _flight_size < _max_inflight){
    retransmit_packet();
  }
  //new packets
  while (q->get_pkts_per_flow(_flow_id) < _max_NIC_pkts && _highest_sent < _flow_size && _flight_size < _max_inflight) {
    //send first burst of packets to NIC, always try to keep _max_NIC_pkts there
    send_packet();
  }
}

Queue* 
HbHSrc::sendToNIC(Packet* pkt) {
    DynExpTopology* top = pkt->get_topology();
    Queue* nic = top->get_queue_serv_tor(pkt->get_src()); // returns pointer to nic queue
    assert(nic);
    nic->receivePacket(*pkt); // send this packet to the nic queue
    return nic; // return pointer so NDP source can update send time
}

//TODO?
void 
HbHSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    return;
}

/* HbH SINK */

HbHSink::HbHSink() 
    : Logged("sink"), _cumulative_ack(0)
{
    _nodename = "hbhsink";
}

void 
HbHSink::connect(HbHSrc& src) {
    _src = &src;
    _cumulative_ack = 0;
}

// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void HbHSink::receivePacket(Packet& pkt) {
  pkt.clear_path();
  HbHPacket *p = (HbHPacket*)(&pkt);
  HbHPacket::seq_t seqno = p->seqno();
  simtime_picosec ts = p->ts();
  bool last_packet = ((HbHPacket*)&pkt)->last_packet();

  assert(pkt.type() == HBHDATA);
  assert(p->size() >= ACKSIZE);

  int size = p->size()-ACKSIZE;
  assert(_src->_flight_size >= size);
  _src->_flight_size -= size;
  _total_hops += pkt.get_crthop();

  __global_network_tot_hops += pkt.get_crthop();
  __global_network_tot_hops_samples++;

  if(pkt.id() == 269245) {
    cout << "SINK RECEIVEPACKET " << _src->_top->time_to_slice(_src->eventlist().now()) << " " << _src->eventlist().now() << endl;
  }

  if(debug_flow(flow_id())){
    cout << "Sink " << _src->flow_id() << " receivePacket seqno " << seqno << " cumack " << _cumulative_ack << " id " << pkt.id() << " " << _src->eventlist().now() << endl;
  }

  if (last_packet) {
    // we've seen the last packet of this flow, but may not have
    // seen all the preceding packets
    //cout << "last_packet " << _src->_flow_id << " seqno " << seqno << " segsize " << size << endl;
    _last_packet_seqno = p->seqno() + size - 1;
  }
  p->free();

  _total_received+=size;

  if (seqno == _cumulative_ack+1) { // it's the next expected seq no
    _cumulative_ack = seqno + size - 1;
    // are there any additional received packets we can now ack?
    while (!_received.empty() && (_received.front().first == _cumulative_ack+1) ) {
      //cout << "xpasssink " << _src->_flow_id << " front: " << _received.front().first << " cumack+1: " << _cumulative_ack+1 << endl;
      _cumulative_ack+= _received.front().second;
      _received.pop_front();
    }
  } else if (seqno < _cumulative_ack+1) {
    //must have been a bad retransmit
  } else { // it's not the next expected sequence number
    if (_received.empty()) {
      _received.push_front({seqno,size});
      } else if (seqno > _received.back().first) { // likely case
      _received.push_back({seqno,size});
      } 
    else { // uncommon case - it fills a hole
      list<pair<uint64_t,uint64_t>>::iterator i;
      for (i = _received.begin(); i != _received.end(); i++) {
        if (seqno == i->first) break; // it's a bad retransmit
        if (seqno < i->first) {
          _received.insert(i, {seqno,size});
          break;
        }
      }
    }
  }

  // have we seen everything yet?
  if (_last_packet_seqno > 0 && _cumulative_ack >= _last_packet_seqno && !_src->_finished) {
    cout << "FCT " << _src->get_flow_src() << " " << _src->get_flow_dst() << " " << _src->get_flowsize() <<
      " " << timeAsMs(_src->eventlist().now() - _src->get_start_time()) << " " << timeAsMs(_src->get_start_time()) << " " << _total_hops << " " << _src->flow_id() << endl;
    _src->_finished = true;
  }
  if(last_packet && !_src->_finished) {
    //cout << "??? cumack " << _cumulative_ack << " lastseqno " << _last_packet_seqno << " flowsize " << _src->_flow_size << " flowid " << _src->_flow_id << endl;
  }
  _src->triggerSending();
}
