// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "dispatcher.h"
#include "datacenter/dynexp_topology.h"
#include "xpass.h"

Flow::Flow(int src, int dst, simtime_picosec start_t, unsigned flowsize) {
  _src = src;
  _dst = dst;
  _start_t = start_t;
  _flowsize = flowsize;
}

XPassFlow::XPassFlow(int src, int dst, simtime_picosec start_t, unsigned flowsize)
  : Flow(src, dst, start_t, flowsize) {}

void
XPassFlow::startFlow(EventList *ev, DynExpTopology *top, void *params) {
  XPassParams *p = (XPassParams*)params;
  _xpsrc = new XPassSrc(*ev, _src, _dst, top);
  _xpsrc->setCwnd(p->cwnd*Packet::data_packet_size());
  _xpsrc->set_flowsize(_flowsize); // bytes
  _xpsink = new XPassSink(*ev, p->w_init, p->tloss);
  _xpsrc->connect(*_xpsink, _start_t);
}
