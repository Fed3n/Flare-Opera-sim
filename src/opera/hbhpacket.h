#ifndef HBHPACKET_H
#define HBHPACKET_H

#include <list>
#include "config.h"
#include "datacenter/dynexp_topology.h"
#include "network.h"

#define ACKSIZE 64
class HbHSrc;
class HbHSink;

static uint64_t packet_id_gen;

class HbHPacket : public Packet {
public:
  typedef uint64_t seq_t;

  // pseudo-constructor for a routeless packet - routing information
  // must be filled in later
  inline static HbHPacket* newpkt(DynExpTopology *top, int src, int dst, HbHSrc *hbhsrc, HbHSink *hbhsink, 
                                    seq_t seqno, int size, bool retransmitted, bool last_packet) {
    HbHPacket* p = _packetdb.allocPacket();
    p->_size = size+ACKSIZE;
    p->_type = HBHDATA;
    p->_is_header = false;
    p->_bounced = false;
    p->_seqno = seqno;
    p->_retransmitted = retransmitted;
    p->_last_packet = last_packet;
    p->_src = src;
    p->_dst = dst;
    p->_hbhsrc = hbhsrc;
    p->_hbhsink = hbhsink;
    p->_top = top;
    p->_queueing = 0;
    p->_next_q = NULL;
    p->_id = packet_id_gen++;
    return p;
  }

  void free() {_packetdb.freePacket(this);}
  virtual ~HbHPacket(){}
  inline bool retransmitted() const {return _retransmitted;}
  inline bool last_packet() const {return _last_packet;}
  inline simtime_picosec ts() const {return _ts;}
  inline void set_ts(simtime_picosec ts) {_ts = ts;}
  inline unsigned seqno() { return _seqno; }
  HbHSrc *get_hbhsrc() {return _hbhsrc;}
  HbHSink *get_hbhsink() {return _hbhsink;}

protected:
  HbHSrc *_hbhsrc;
  HbHSink *_hbhsink;
  simtime_picosec _ts;
  seq_t _seqno;
  bool _retransmitted;
  bool _last_packet;  // set to true in the last packet in a flow.
  static PacketDB<HbHPacket> _packetdb;
};

#endif
