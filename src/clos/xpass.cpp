// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-    
#include <cstdint>
#include <cstdlib>
#include <math.h>
#include <iostream>
#include "config.h"
#include "creditqueue.h"
#include "eventlist.h"
#include "xpass.h"
#include "queue.h"
#include "xpasspacket.h"
#include <stdio.h>
#include <type_traits>

////////////////////////////////////////////////////////////////
//  XPASS SOURCE
////////////////////////////////////////////////////////////////

/* When you're debugging, sometimes it's useful to enable debugging on
   a single XPASS receiver, rather than on all of them.  Set this to the
   node ID and recompile if you need this; otherwise leave it
   alone. */
//#define LOGSINK 2332
#define LOGSINK 0

/* We experimented with adding extra pulls to cope with scenarios
   where you've got a bad link and pulls get dropped.  Generally you
   don't want to do this though, so best leave RCV_CWND set to
   zero. Lost pulls are well handled by the cumulative pull number. */
//#define RCV_CWND 15
#define RCV_CWND 0

int XPassSrc::_global_node_count = 0;
/* _rtt_hist is used to build a histogram of RTTs.  The index is in
   units of microseconds, and RTT is from when a packet is first sent
   til when it is ACKed, including any retransmissions.  You can read
   this out after the sim has finished if you care about this. */
int XPassSrc::_rtt_hist[10000000] = {0};

/* keep track of RTOs.  Generally, we shouldn't see RTOs if
   return-to-sender is enabled.  Otherwise we'll see them with very
   large incasts. */
uint32_t XPassSrc::_global_rto_count = 0;

/* _min_rto can be tuned using SetMinRTO. Don't change it here.  */
simtime_picosec XPassSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_RTO_MIN);


XPassSrc::XPassSrc(EventList &eventlist, int flow_src, int flow_dst)
: EventSource(eventlist,"xpass"), _flow(NULL), _flow_src(flow_src), _flow_dst(flow_dst)
{
  _mss = Packet::data_packet_size();

  _base_rtt = timeInf;
  _acked_packets = 0;
  _packets_sent = 0;
  _new_packets_sent = 0;
  _rtx_packets_sent = 0;
  _acks_received = 0;
  _nacks_received = 0;
  _pulls_received = 0;
  _implicit_pulls = 0;
  _bounces_received = 0;

  _flight_size = 0;

  _highest_sent = 0;
  _last_acked = 0;
  _finished = false;

  _sink = 0;

  _rtt = 0;
  _rto = timeFromMs(1);
  _cwnd = 1 * Packet::data_packet_size();
  _mdev = 0;
  _drops = 0;
  _flow_size = ((uint64_t)1)<<63;
  _last_pull = 0;
  _pull_window = 0;

  _crt_path = 0; // used for SCATTER_PERMUTE route strategy

  _feedback_count = 0;
  _rtx_timeout_pending = false;
  _rtx_timeout = timeInf;
  _node_num = _global_node_count++;
  _nodename = "xpasssrc" + to_string(_node_num);

  // debugging hack
  _log_me = false;
}

void XPassSrc::set_flowsize(uint64_t flow_size_in_bytes) {

  _flow_size = flow_size_in_bytes;
  if (_flow_size < _mss)
    _pkt_size = _flow_size;
  else
    _pkt_size = _mss;
}

void XPassSrc::set_traffic_logger(TrafficLogger* pktlogger) {
  _flow.set_logger(pktlogger);
}

void XPassSrc::log_me() {
  // avoid looping
  if (_log_me == true)
    return;
  cout << "Enabling logging on XPassSrc " << _nodename << endl;
  _log_me = true;
  if (_sink)
    _sink->log_me();
}

void XPassSrc::set_paths(vector<const Route*>* rt_list){
  int no_of_paths = rt_list->size();

  _paths.resize(no_of_paths);
  _original_paths.resize(no_of_paths);
  _path_acks.resize(no_of_paths);
  _path_nacks.resize(no_of_paths);
  _bad_path.resize(no_of_paths);
  _avoid_ratio.resize(no_of_paths);
  _avoid_score.resize(no_of_paths);
  _path_counts_new.resize(no_of_paths);
  _path_counts_rtx.resize(no_of_paths);
  _path_counts_rto.resize(no_of_paths);

  for (unsigned int i=0; i < no_of_paths; i++){
    Route* tmp = new Route(*(rt_list->at(i)));
    tmp->add_endpoints(this, _sink);
    tmp->set_path_id(i, rt_list->size());
    _paths[i] = tmp;
    _original_paths[i] = tmp;
    _path_counts_new[i] = 0;
    _path_counts_rtx[i] = 0;
    _path_counts_rto[i] = 0;
    _path_acks[i] = 0;
    _path_nacks[i] = 0;
    _avoid_ratio[i] = 0;
    _avoid_score[i] = 0;
    _bad_path[i] = false;
    _crt_path = 0;
  }
}

void XPassSrc::startflow(){
  //cout << "startflow\n";
  _highest_sent = 0;
  _last_acked = 0;

  _acked_packets = 0;
  _packets_sent = 0;
  _rtx_timeout_pending = false;
  _rtx_timeout = timeInf;
  _pull_window = 0;

  _flight_size = 0;
  _first_window_count = 0;
  while (_flight_size < _cwnd && _flight_size < _flow_size) {
    send_packet(0);
    _first_window_count++;
  }
}

void XPassSrc::connect(Route& routeout, Route& routeback, XPassSink& sink, simtime_picosec starttime) {
  _route = &routeout;
  assert(_route);

  _sink = &sink;
  _flow.id = id; // identify the packet flow with the XPASS source that generated it
  _flow._name = _name;
  _sink->connect(*this, routeback);

  set_start_time(starttime); // record the start time in _start_time

  //cout << "Flow ndpsrc" << flow_id() << " (name:" << _name << ") set to start at " << timeAsMs(starttime) << " ms" << endl;
  //cout << "FlowID " << id << " name " << _name << " SetStartms " << timeAsMS(starttime) << endl;
  //cout << "flowID " << id << " bytes " << get_flowsize() << " set_start_ms " << timeAsMs(starttime) << endl;
  eventlist().sourceIsPending(*this,starttime);

  //debugging hacks
  if (sink.get_id()==LOGSINK) {
    cout << "Found source for " << LOGSINK << "\n";
    _log_me = true;
  }
}

#define ABS(X) ((X)>0?(X):-(X))

void XPassSrc::receivePacket(Packet& pkt) 
{
  //cout << "xpasssrc " << flow_id() << " receivePacket " << eventlist().now() << endl;
  switch (pkt.type()) {
    case XPCREDIT: 
      {
        _pulls_received++;
        _pull_window--;
        if (_log_me) {
          printf("PULL, pw=%d\n", _pull_window);
        }
        XPassPull *p = (XPassPull*)(&pkt);
        XPassPull::seq_t cum_ackno = p->cumulative_ack();
        if (cum_ackno > _last_acked) { // a brand new ack    
          // we should probably cancel the rtx timer for any acked by
          // the cumulative ack, but we'll get an ACK or NACK anyway in
          // due course.
          _last_acked = cum_ackno;
          update_rtx_time();
        }
        //printf("Receive PULL: %s\n", p->pull_bitmap().to_string().c_str());
        pull_packets(p->pullno(), p->pacerno());
        return;
      }
    default: assert(0);
  }
}


void XPassSrc::pull_packets(XPassPull::seq_t pull_no, XPassPull::seq_t pacer_no) {
  send_packet(pacer_no);
  _last_pull++;
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
void XPassSrc::send_packet(XPassPull::seq_t pacer_no) {
  XPassPacket* p;
  if (!_rtx_queue.empty()) {
    // There are packets in the RTX queue for us to send

    p = _rtx_queue.front();
    _rtx_queue.pop_front();
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_SEND);
    p->set_ts(eventlist().now());
    p->set_pacerno(pacer_no);

    p->set_route(*_route);
    PacketSink* sink = p->sendOn();
    NICCreditQueue *q = dynamic_cast<NICCreditQueue*>(sink);
    assert(q);
    //Figure out how long before the feeder queue sends this
    //packet, and add it to the sent time. Packets can spend quite
    //a bit of time in the feeder queue.  It would be better to
    //have the feeder queue update the sent time, because the
    //feeder queue isn't a FIFO but that would be hard to
    //implement in a real system, so this is a rough proxy.
    uint32_t service_time = q->serviceTime();  
    _sent_times[p->seqno()] = eventlist().now() + service_time;
    _packets_sent ++;
    _rtx_packets_sent++;
    update_rtx_time();
    if (_rtx_timeout == timeInf) {
      _rtx_timeout = eventlist().now() + _rto;
    }
  } else {
    // there are no packets in the RTX queue, so we'll send a new one
    bool last_packet = false;
    if (_flow_size) {
      if (_highest_sent >= _flow_size) {
        /* we've sent enough new data. */
        /* xxx should really make the last packet sent be the right size
     * if _flow_size is not a multiple of _mss */
        return;
      } 
      if (_highest_sent + _pkt_size >= _flow_size) {
        last_packet = true;
      }
    }
    p = XPassPacket::newpkt(_flow,_highest_sent+1, pacer_no, _pkt_size, false, last_packet);
    p->set_route(*_route);
    p->set_ts(eventlist().now());

    _flight_size += _pkt_size;
    // 	if (_log_me) {
    // 	    cout << "Sent " << _highest_sent+1 << " FSz: " << _flight_size << endl;
    // 	}
    _highest_sent += _pkt_size;  //XX beware wrapping
    _packets_sent++;
    _new_packets_sent++;

    PacketSink* sink = p->sendOn();
    NICCreditQueue *q = dynamic_cast<NICCreditQueue*>(sink);
    assert(q);
    //Figure out how long before the feeder queue sends this
    //packet, and add it to the sent time. Packets can spend quite
    //a bit of time in the feeder queue.  It would be better to
    //have the feeder queue update the sent time, because the
    //feeder queue isn't a FIFO but that would be hard to
    //implement in a real system, so this is a rough proxy.
    uint32_t service_time = q->serviceTime();  
    //cout << "service_time2: " << service_time << endl;
    _sent_times[p->seqno()] = eventlist().now() + service_time;
    _first_sent_times[p->seqno()] = eventlist().now();

    if (_rtx_timeout == timeInf) {
      _rtx_timeout = eventlist().now() + _rto;
    }
  }
}

void 
XPassSrc::update_rtx_time() {
  //simtime_picosec now = eventlist().now();
  if (_sent_times.empty()) {
    _rtx_timeout = timeInf;
    return;
  }
  map<XPassPacket::seq_t, simtime_picosec>::iterator i;
  simtime_picosec first_senttime = timeInf;
  int c = 0;
  for (i = _sent_times.begin(); i != _sent_times.end(); i++) {
    simtime_picosec sent = i->second;
    if (sent < first_senttime || first_senttime == timeInf) {
      first_senttime = sent;
    }
    c++;
  }
  _rtx_timeout = first_senttime + _rto;
}

void 
XPassSrc::process_cumulative_ack(XPassPacket::seq_t cum_ackno) {
  map<XPassPacket::seq_t, simtime_picosec>::iterator i, i_next;
  i = _sent_times.begin();
  while (i != _sent_times.end()) {
    if (i->first <= cum_ackno) {
      i_next = i; //juggling to keep i valid
      i_next++;
      _sent_times.erase(i);
      i = i_next;
      } else {
      return;
    }
  }
  //need to call update_rtx_time right after this!
}

void 
XPassSrc::retransmit_packet() {
  //cout << "starting retransmit_packet\n";
  XPassPacket* p;
  map<XPassPacket::seq_t, simtime_picosec>::iterator i, i_next;
  i = _sent_times.begin();
  list <XPassPacket::seq_t> rtx_list;
  // we build a list first because otherwise we're adding to and
  // removing from _sent_times and the iterator gets confused
  while (i != _sent_times.end()) {
    if (i->second + _rto <= eventlist().now()) {
      //cout << "_sent_time: " << timeAsUs(i->second) << "us rto " << timeAsUs(_rto) << "us now " << timeAsUs(eventlist().now()) << "us\n";
      //this one is due for retransmission
      rtx_list.push_back(i->first);
      i_next = i; //we're about to invalidate i when we call erase
      i_next++;
      _sent_times.erase(i);
      i = i_next;
    } else {
      i++;
    }
  }
  list <XPassPacket::seq_t>::iterator j;
  for (j = rtx_list.begin(); j != rtx_list.end(); j++) {
    XPassPacket::seq_t seqno = *j;
    bool last_packet = (seqno + _pkt_size - 1) >= _flow_size;
    p = XPassPacket::newpkt(_flow, seqno, 0, _pkt_size, true, last_packet);
    p->set_route(*_route);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    //_sent_times[seqno] = eventlist().now();
    // 	if (_log_me) {
    //cout << "Sent " << seqno << " RTx" << " flow id " << p->flow().id << endl;
    // 	}
    _global_rto_count++;
    cout << "Total RTOs: " << _global_rto_count << endl;
    _path_counts_rto[p->path_id()]++;
    p->sendOn();
    _packets_sent++;
    _rtx_packets_sent++;
  }
  update_rtx_time();
}

void XPassSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
#ifndef RESEND_ON_TIMEOUT
  return;  // if we're using RTS, we shouldn't need to also use
  // timeouts, at least in simulation where we don't see
  // corrupted packets
#endif

  if (_highest_sent == 0) return;
  if (_rtx_timeout==timeInf || now + period < _rtx_timeout) return;

  cout <<"At " << timeAsUs(now) << "us RTO " << timeAsUs(_rto) << "us MDEV " << timeAsUs(_mdev) << "us RTT "<< timeAsUs(_rtt) << "us SEQ " << _last_acked / _mss << " CWND "<< _cwnd/_mss << " Flow ID " << str()  << endl;
  /*
    if (_log_me) {
  cout << "Flow " << LOGSINK << "scheduled for RTX\n";
    }
    */

  // here we can run into phase effects because the timer is checked
  // only periodically for ALL flows but if we keep the difference
  // between scanning time and real timeout time when restarting the
  // flows we should minimize them !
  if(!_rtx_timeout_pending) {
    _rtx_timeout_pending = true;


    // check the timer difference between the event and the real value
    simtime_picosec too_early = _rtx_timeout - now;
    if (now > _rtx_timeout) {
      // this shouldn't happen
      cout << "late_rtx_timeout: " << _rtx_timeout << " now: " << now << " now+rto: " << now + _rto << " rto: " << _rto << endl;
      too_early = 0;
    }
    eventlist().sourceIsPendingRel(*this, too_early);
  }
}

void XPassSrc::log_rtt(simtime_picosec sent_time) {
  int64_t rtt = eventlist().now() - sent_time;
  if (rtt >= 0) 
    _rtt_hist[(int)timeAsUs(rtt)]++;
  else
    cout << "Negative RTT: " << rtt << endl;
}

void XPassSrc::doNextEvent() {
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

void XPassSrc::print_stats() {
  cout << _nodename << "\n";
  int total_new = 0, total_rtx = 0, total_rto = 0;
  for (int i = 0; i < _paths.size(); i++) {
    cout << _path_counts_new[i] << "/" << _path_counts_rtx[i] << "/" << _path_counts_rto[i] << " ";
    total_new += _path_counts_new[i];
    total_rtx += _path_counts_rtx[i];
    total_rto += _path_counts_rto[i];
  }
  cout << "\n";
  cout << "New: " << total_new << "  RTX: " << total_rtx << "  RTO " << total_rto << "\n";
}

////////////////////////////////////////////////////////////////
//  XPASS SINK
////////////////////////////////////////////////////////////////

XPassSink::XPassSink(EventList& eventlist, double w_init, double target_loss) : EventSource(eventlist, "sink"), _cumulative_ack(0) , _total_received(0) 
{
  _src = 0;
  _nodename = "ndpsink";
  _pull_no = 0;
  _last_packet_seqno = 0;
  _last_packet_pacerno = 0;
  _log_me = false;
  _total_received = 0;
  _max_rate = speedFromMbps(uint64_t(95000));
  _crt_rate = _max_rate*w_init;
  _min_rate = speedFromMbps((uint64_t)100);
  _max_weight = 0.5;
  _weight = _max_weight;
  _min_weight = 0.01;
  _target_loss = target_loss;
  _is_increasing = true;
  _last_fb_update = 0;
  _recvd_data = 0;
  _last_tp_sample_t = 0;
}

void XPassSink::log_me() {
  // avoid looping
  if (_log_me == true)
    return;

  _log_me = true;
  if (_src)
    _src->log_me();

}

/* Connect a src to this sink.  We normally won't use this route if
   we're sending across multiple paths - call set_paths() after
   connect to configure the set of paths to be used. */
void XPassSink::connect(XPassSrc& src, Route& route)
{
  _src = &src;
  _route = &route;

  _cumulative_ack = 0;
  _drops = 0;

  // debugging hack
  if (get_id() == LOGSINK) {
    cout << "Found sink for " << LOGSINK << "\n";
    _log_me = true;
  }
}

/* sets the set of paths to be used when sending from this XPassSink back to the XPassSrc */
void XPassSink::set_paths(vector<const Route*>* rt_list){
  assert(_paths.size() == 0);
  _paths.resize(rt_list->size());
  for (unsigned int i=0;i<rt_list->size();i++){
    Route* t = new Route(*(rt_list->at(i)));
    t->add_endpoints(this, _src);
    _paths[i]=t;
  }
  _crt_path = 0;
}

// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void XPassSink::receivePacket(Packet& pkt) {
  XPassPacket *p = (XPassPacket*)(&pkt);
  XPassPacket::seq_t seqno = p->seqno();
  XPassPacket::seq_t pacer_no = p->pacerno();
  simtime_picosec ts = p->ts();
  bool last_packet = ((XPassPacket*)&pkt)->last_packet();
  _recvd_data += p->size()-64;
  if(eventlist().now() - _last_tp_sample_t > 2E9) {
    //cout << "TP " << flow_id() << " " << _recvd_data*8/1E6 << " " << eventlist().now() << endl;  
    _recvd_data = 0;
    _last_tp_sample_t += 1E9;
  }
  assert(pkt.type() == XPDATA);
  //cout << "xpasssink receivePacket seqno " << seqno << " " << eventlist().now() << endl;


  int size = p->size()-ACKSIZE;

  if(_total_received <= 0) { //first packet
    //start pulling 
    eventlist().sourceIsPendingRel(*this, 0); 
  }

  if (last_packet) {
    // we've seen the last packet of this flow, but may not have
    // seen all the preceding packets
    _last_packet_seqno = p->seqno() + size - 1;
  }
  p->free();

  updateRTT(ts);

  if(pacer_no > _last_packet_pacerno) {
    assert(pacer_no > _last_packet_pacerno);
    long distance = pacer_no - _last_packet_pacerno;
    _tot_credits += distance;
    //credit number gap is counted as lost credits (TODO: will break when there's reordering)
    if(distance > 0) {
      _drop_credits += distance-1;
    }
    _last_packet_pacerno = pacer_no;
  }

  _total_received+=size;
  if (seqno == _cumulative_ack+1) { // it's the next expected seq no
    _cumulative_ack = seqno + size - 1;
    // are there any additional received packets we can now ack?
    while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
      _received.pop_front();
      _cumulative_ack+= size;
    }
  } else if (seqno < _cumulative_ack+1) {
    //must have been a bad retransmit
  } else { // it's not the next expected sequence number
    if (_received.empty()) {
      _received.push_front(seqno);
      //it's a drop in this simulator there are no reorderings.
      _drops += (size + seqno-_cumulative_ack-1)/size;
      } else if (seqno > _received.back()) { // likely case
      _received.push_back(seqno);
      } 
    else { // uncommon case - it fills a hole
      list<uint64_t>::iterator i;
      for (i = _received.begin(); i != _received.end(); i++) {
        if (seqno == *i) break; // it's a bad retransmit
        if (seqno < (*i)) {
          _received.insert(i, seqno);
          break;
        }
      }
    }
  }
  //update rate based on credit loss each rtt
  if(eventlist().now()-_last_fb_update > _rtt) {
    feedbackControl();
    _last_fb_update = eventlist().now();
  }
  //send_ack(ts, seqno, pacer_no);
  // have we seen everything yet?
  if (_last_packet_seqno > 0 && _cumulative_ack == _last_packet_seqno) {
    cout << "FCT " << _src->get_flow_src() << " " << _src->get_flow_dst() << " " << _src->get_flowsize() <<
      " " << timeAsMs(_src->eventlist().now() - _src->get_start_time()) << " " << timeAsMs(_src->get_start_time()) << endl;
    _src->_finished = true;
  }
}

void
XPassSink::updateRTT(simtime_picosec ts) {
  //compute rtt
  uint64_t m = (eventlist().now()-ts)*2;
  if (m!=0){
    _max_rtt = m > _max_rtt ? m : _max_rtt;
    if (_rtt>0){
      _rtt = 7*_rtt/8 + m/8;
    } else {
      _rtt = m;
    }
  }
}

void
XPassSink::feedbackControl() {
  if(_tot_credits <= 0) {
    return;
  }
  double credit_loss = (double)_drop_credits / _tot_credits;
  double target_loss = (1.0 - _crt_rate/_max_rate) * _target_loss;
  assert(_drop_credits <= _tot_credits);
  if(credit_loss < target_loss) {
    if(_is_increasing){
      _weight = (_weight+_max_weight)/2;
      _weight = min(_weight, _max_weight);
    }
    _crt_rate = (1-_weight)*_crt_rate + _weight*_max_rate;//*(1+_target_loss);
    _crt_rate = min(_crt_rate, _max_rate);
    _is_increasing = true;
  } else {
    _crt_rate = _crt_rate * (1-credit_loss)*(1+target_loss);
    _crt_rate = max(_crt_rate, _min_rate);
    _weight = max(_weight/2, _min_weight);
    _is_increasing = false;
  }
  _tot_credits = 0;
  _drop_credits = 0;
  //cout << "Flow " << flow_id() << " loss " << credit_loss << " rate " << _crt_rate/1E9 <<  " t " << eventlist().now() << " rtt " << _rtt << endl;
}

simtime_picosec
XPassSink::nextCreditWait() {
  simtime_picosec spacing = (simtime_picosec)((Packet::data_packet_size()+64) 
      * (pow(10.0,12.0) * 8) / _crt_rate);
  //cout << "flow " << _src->flow_id() << " spacing " << spacing << " crt_rate " << _crt_rate << endl;
  //long jitter = 48000 - rand()%96000;
  long jitter = 4000 - rand()%8000;
  //long jitter = 0;
  //cout << "spacing " << spacing+jitter << endl;
  return spacing+jitter;
}

void
XPassSink::doNextEvent() {
  XPassPull *p = XPassPull::newpkt(_src->_flow, *(_route), this);
  p->set_pacerno(_credit_counter++);
  p->set_ackno(p->get_sink()->cumulative_ack());
  p->sendOn();
  if(!_src->_finished) {
    eventlist().sourceIsPendingRel(*this, nextCreditWait());
  }
}

double* XPassPullPacer::_pull_spacing_cdf = NULL;
int XPassPullPacer::_pull_spacing_cdf_count = 0;


/* Every XPassSink needs an XPassPullPacer to pace out its PULL packets.
   Multiple incoming flows at the same receiving node must share a
   single pacer */
XPassPullPacer::XPassPullPacer(EventList& event, double pull_rate_modifier)  : 
  EventSource(event, "ndp_pacer"), _last_pull(0)
{
  _packet_drain_time = (simtime_picosec)((Packet::data_packet_size()+64) * (pow(10.0,12.0) * 8) / speedFromMbps((uint64_t)100000))/pull_rate_modifier;
  _log_me = false;
  _pacer_no = 0;
}

void XPassPullPacer::log_me() {
  // avoid looping
  if (_log_me == true)
    return;

  _log_me = true;
  _total_excess = 0;
  _excess_count = 0;
}

void XPassPullPacer::enqueue_pulls(XPassSink *receiver) {
  bool queueWasEmpty = _pull_queue.empty();
  int n_pulls = (receiver->_src->get_flowsize()/(Packet::data_packet_size()))*10;
  for (int i = 0; i < n_pulls; i++) {
    XPassPull *p = XPassPull::newpkt(receiver->_src->_flow, *(receiver->_route), receiver);
    _pull_queue.enqueue(*p);
  }
  simtime_picosec delta = eventlist().now()-_last_pull;
  //cout << _packet_drain_time << " " << delta << " " << _packet_drain_time - delta << endl;
  if (queueWasEmpty && delta >= _packet_drain_time){
    eventlist().sourceIsPendingRel(*this, 0);
  }
}

void XPassPullPacer::set_pacerno(Packet *pkt, XPassPull::seq_t pacer_no) {
  assert(pkt->type() == XPCREDIT);
  ((XPassPull*)pkt)->set_pacerno(pacer_no);
}

void XPassPullPacer::sendPacket(Packet* ack, XPassPacket::seq_t rcvd_pacer_no, XPassSink* receiver) {
  if (rcvd_pacer_no != 0 && _pacer_no - rcvd_pacer_no < RCV_CWND) {
    receiver->increase_window();
  }

  simtime_picosec drain_time;

  assert(_packet_drain_time > 0);
  drain_time = _packet_drain_time;


  if (_pull_queue.empty()){
    simtime_picosec delta = eventlist().now()-_last_pull;

    if (delta >= drain_time){
      //send out as long as last NACK/ACK was sent more than packetDrain time ago.
      ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_SEND);
      if (_log_me) {
        double excess = (delta - drain_time)/(double)drain_time;
        _total_excess += excess;
        _excess_count++;
        /*		cout << "Mean excess: " << _total_excess / _excess_count << endl;
    if (ack->type() == XPASSACK) {
        cout << "Ack " <<  (((XPassAck*)ack)->ackno()-1)/9000 << " excess " << excess << " (no queue)\n";
    } else if (ack->type() == XPASSNACK) {
        cout << "Nack " << (((XPassNack*)ack)->ackno()-1)/9000 << " excess " << excess << " (no queue)\n";
    } else {
        cout << "WTF\n";
        }*/
      }
      set_pacerno(ack, _pacer_no++);
      ack->sendOn();
      _last_pull = eventlist().now();
      return;
      } else {
      eventlist().sourceIsPendingRel(*this,drain_time - delta);
    }
  }

  //Create a pull packet and stick it in the queue.
  //Send on the ack/nack, but with pull cleared.
  XPassPull *pull_pkt = NULL;
  //TODO create pull_pkt
  _pull_queue.enqueue(*pull_pkt);

  ack->sendOn();
}


// when we're reached the last packet of a connection, we can release
// all the queued acks for that connection because we know they won't
// generate any more data packets.  This will move the nacks up the
// queue too, causing any retransmitted packets from the tail of the
// file to be received earlier
void XPassPullPacer::release_pulls(uint32_t flow_id) {
  //cout << "release pulls\n";
  _pull_queue.flush_flow(flow_id);
}


void XPassPullPacer::doNextEvent(){
  //cout << "pullpacer doNextEvent\n";
  if (_pull_queue.empty()) {
    // this can happen if we released all the acks at the end of
    // the connection.  we didn't cancel the timer, so we end up
    // here.
    return;
  }

  Packet *pkt = _pull_queue.dequeue();
  assert(pkt->type() == XPCREDIT);
  XPassPull *p = (XPassPull*)pkt;

  //cout << "send credit " << eventlist().now() << endl;
  set_pacerno(pkt, _pacer_no++);
  p->set_ackno(p->get_sink()->cumulative_ack());
  pkt->sendOn();
  
  _last_pull = eventlist().now();

  simtime_picosec drain_time;

  if (_packet_drain_time>0)
    drain_time = _packet_drain_time;
  else {
    int t = (int)(drand()*_pull_spacing_cdf_count);
    drain_time = 10*timeFromNs(_pull_spacing_cdf[t])/20;
    //cout << "Drain time is " << timeAsUs(drain_time);
  }

  if (!_pull_queue.empty()){
    eventlist().sourceIsPendingRel(*this,drain_time);//*(0.5+drand()));
  }
  else {
    //    cout << "Empty pacer queue at " << timeAsMs(eventlist().now()) << endl; 
  }
}


////////////////////////////////////////////////////////////////
//  XPASS RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

XPassRtxTimerScanner::XPassRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
  : EventSource(eventlist,"RtxScanner"), 
  _scanPeriod(scanPeriod)
{
  eventlist.sourceIsPendingRel(*this, 0);
}

void 
XPassRtxTimerScanner::registerXPass(XPassSrc &tcpsrc)
{
  _tcps.push_back(&tcpsrc);
}

void
XPassRtxTimerScanner::doNextEvent() 
{
  simtime_picosec now = eventlist().now();
  tcps_t::iterator i;
  for (i = _tcps.begin(); i!=_tcps.end(); i++) {
    (*i)->rtx_timer_hook(now,_scanPeriod);
  }
  eventlist().sourceIsPendingRel(*this, _scanPeriod);
}

