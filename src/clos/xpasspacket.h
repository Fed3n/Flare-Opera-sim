// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef XPASSPACKET_H
#define XPASSPACKET_H

#include <list>
#include "network.h"

// XPassPacket and XPassAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new XPassPacket or XPassAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

#define ACKSIZE 64
#define VALUE_NOT_SET -1
#define PULL_MAXPATHS 256 // maximum path ID that can be pulled
class XPassSink;

class XPassPacket : public Packet {
public:
  typedef uint64_t seq_t;

  // pseudo-constructor for a routeless packet - routing information
  // must be filled in later
  inline static XPassPacket* newpkt(PacketFlow &flow, 
                                    seq_t seqno, seq_t pacerno, int size, 
                                    bool retransmitted, 
                                    bool last_packet) {
    XPassPacket* p = _packetdb.allocPacket();
    p->set_attrs(flow, size+ACKSIZE, seqno+size-1); // The XPASS sequence number is the first byte of the packet; I will ID the packet by its last byte.
    p->_type = XPDATA;
    p->_is_header = false;
    p->_bounced = false;
    p->_seqno = seqno;
    p->_pacerno = pacerno;
    p->_retransmitted = retransmitted;
    p->_last_packet = last_packet;
    p->_path_len = 0;
    return p;
  }

  void free() {_packetdb.freePacket(this);}
  virtual ~XPassPacket(){}
  inline seq_t seqno() const {return _seqno;}
  inline seq_t pacerno() const {return _pacerno;}
  inline void set_pacerno(seq_t pacerno) {_pacerno = pacerno;}
  inline bool retransmitted() const {return _retransmitted;}
  inline bool last_packet() const {return _last_packet;}
  inline simtime_picosec ts() const {return _ts;}
  inline void set_ts(simtime_picosec ts) {_ts = ts;}
  inline int32_t path_id() const {return _route->path_id();}
  inline int32_t no_of_paths() const {return _no_of_paths;}

protected:
  seq_t _seqno;
  seq_t _pacerno;  // the pacer sequence number from the pull, seq space is common to all flows on that pacer
  simtime_picosec _ts;
  bool _retransmitted;
  int32_t _no_of_paths;  // how many paths are in the sender's
  // list.  A real implementation would not
  // send this in every packet, but this is
  // simulation, and this is easiest to
  // implement
  bool _last_packet;  // set to true in the last packet in a flow.
  static PacketDB<XPassPacket> _packetdb;
};

class XPassPull : public Packet {
public:
  typedef XPassPacket::seq_t seq_t;
  inline static XPassPull* newpkt(PacketFlow &flow, const Route &route, XPassSink *sink) {
    XPassPull* p = _packetdb.allocPacket();
    p->set_route(flow, route, ACKSIZE, 0);
    p->set_flow_id(flow.flow_id());
    p->_sink = sink;
    p->_type = XPCREDIT;
    p->_is_header = true;
    p->_bounced = false;
    //p->_ackno = ack->ackno();
    //p->_cumulative_ack = ack->cumulative_ack();
    //p->_pullno = ack->pullno();
    p->_path_len = 0;
    return p;
  }

  void free() {_packetdb.freePacket(this);}
  inline seq_t pacerno() const {return _pacerno;}
  inline void set_pacerno(seq_t pacerno) {_pacerno = pacerno;}
  inline seq_t ackno() const {return _ackno;}
  inline void set_ackno(seq_t ackno) {_ackno = ackno;}
  inline seq_t cumulative_ack() const {return _cumulative_ack;}
  inline seq_t pullno() const {return _pullno;}
  int32_t path_id() const {return _path_id;}
  XPassSink *get_sink() {return _sink;}

  virtual ~XPassPull(){}

protected:
  seq_t _pacerno;
  seq_t _ackno;
  seq_t _cumulative_ack;
  seq_t _pullno;
  int32_t _path_id; // indicates ??
  XPassSink* _sink;
  static PacketDB<XPassPull> _packetdb;
};

#endif
