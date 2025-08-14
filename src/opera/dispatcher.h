// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "config.h"
#include "datacenter/dynexp_topology.h"
#include "eventlist.h"
#include "xpass.h"

class Flow {
public:
  Flow(int src, int dst, simtime_picosec start_t, unsigned flowsize);
  virtual void startFlow(EventList *ev, DynExpTopology *top, void* params);
protected:
  int _src, _dst;
  simtime_picosec _start_t;
  unsigned _flowsize;
};

struct XPassParams {
  uint32_t cwnd;
  double w_init;
  double tloss;
};

class XPassFlow: public Flow {
public:
  XPassFlow(int src, int dst, simtime_picosec start_t, unsigned flowsize);
  virtual void startFlow(EventList *ev, DynExpTopology *top, void* params);
private:
  XPassSrc *_xpsrc;
  XPassSink *_xpsink;
};

class FlowDispatcher : public EventSource {
public:
  FlowDispatcher();
  void addFlow();
  virtual void doNextEvent();
  virtual void setParams();

private:
  //assume inserted in order
  vector<Flow> _flows;

  DynExpTopology *_top;
};

class XPassFlowDispatcher : public FlowDispatcher {
public:
  XPassFlowDispatcher();
  virtual void setParams();

private:
  XPassParams *params;
};

#endif
