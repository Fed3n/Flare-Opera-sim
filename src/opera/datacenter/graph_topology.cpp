// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "graph_topology.h"
#include <vector>
#include "dynexp_topology.h"
#include "string.h"
#include <sstream>
#include <strstream>
#include <iostream>
#include <fstream> // to read from file
#include <map>
#include "main.h"
#include "queue.h"
#include "pipe.h"
#include "compositequeue.h"
#include "ecnqueue.h"
#include "intqueue.h"
#include "creditqueue.h"
#include "ecn.h"
//#include "prioqueue.h"

#include "rlbmodule.h"

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

string ntoa(double n);
string itoa(uint64_t n);

GraphTopology::GraphTopology(mem_b queuesize, Logfile* lg, EventList* ev,queue_type q, string topfile, map<string,uint64_t> params)
  : DynExpTopology(queuesize, lg, ev, q, params)
{
    _queuesize = queuesize;
    logfile = lg;
    eventlist = ev;
    qt = q;

    read_params(topfile);
 
    set_params();

    init_network();
}

GraphTopology::GraphTopology(mem_b queuesize, Logfile* lg, EventList* ev,queue_type q, string topfile) 
  : DynExpTopology(queuesize, lg, ev, q, (map<string,uint64_t>){})
{
    GraphTopology(queuesize, lg, ev, q, topfile, (map<string,uint64_t>){});
}

// read the topology info from file (generated in Matlab)
void GraphTopology::read_params(string topfile) {

  ifstream input(topfile);

  if (input.is_open()){

    // read the first line of basic parameters:
    string line;
    getline(input, line);
    stringstream stream(line);
    stream >> _ntor;
    stream >> _ndl;
    _no_of_nodes = _ntor*_ndl;

    int node;
    _adjacency.resize(_ntor);
    for (int i = 0; i < _ntor; i++) {
      int port = _ndl;
      getline(input, line);
      stringstream stream(line);
      while(stream >> node){
        _adjacency[i].push_back(node);
        _adj_to_port[{i,node}] = port++;
      }
    }


    // get label switched paths (rest of file)
    _lbls.resize(_ntor);
    for (int i = 0; i < _ntor; i++) {
      _lbls[i].resize(_ntor);
    }

    _connected_slices.resize(_ntor);
    for (int i = 0; i < _ntor; i++) {
      _connected_slices[i].resize(_ntor);
    }

    for (int i = 0; i < _nslice; i++) {
      for (int j = 0; j < _nul*_ntor; j++) {
        int src_tor = uplink_to_tor(j);
        int dst_tor = _adjacency[i][j];
        if(dst_tor >= 0)
            _connected_slices[src_tor][dst_tor].push_back(make_pair(i, uplink_to_port(j)));
      }
    }

    // debug:
    cout << "Loading topology..." << endl;
    int sz = 0;
    while(!input.eof()) {
      int s, d; // current source and destination tor
      vector<int> vtemp;
      int temp;
      getline(input, line);
      if (line.length() <= 0) continue;
      stringstream stream(line);
      while (stream >> temp){
        vtemp.push_back(temp);
      }
      s = vtemp[0]; // current source
      d = vtemp[1]; // current dest
      sz = _lbls[s][d].size();
      _lbls[s][d].resize(sz + 1);
      //starting from the source, push each port on the path
      int crt = s;
      for (int i = 2; i < vtemp.size(); i++) {
        //[crt_tor][nxt_tor]->port
        int port = _adj_to_port[{crt, vtemp[i]}];
        _lbls[s][d][sz].push_back(port);
        //we moved to the next tor
        crt = vtemp[i];
      }
    }

    // debug:
    cout << "Loaded topology." << endl;

  } else {
    cout << "Could not open topology file\n"; exit(1);
  }
}

// set number of possible pipes and queues
void GraphTopology::set_params() {

    pipes_serv_tor.resize(_no_of_nodes); // servers to tors
    queues_serv_tor.resize(_no_of_nodes);

    rlb_modules.resize(_no_of_nodes);

    pipes_tor.resize(_ntor, vector<Pipe*>(_ndl+_nul)); // tors
    queues_tor.resize(_ntor, vector<Queue*>(_ndl+_nul));
}

// initializes all the pipes and queues in the Topology
void GraphTopology::init_network() {
  QueueLoggerSampling* queueLogger;

  // initialize server to ToR pipes / queues
  for (int j = 0; j < _no_of_nodes; j++) { // sweep nodes
    rlb_modules[j] = NULL;
    queues_serv_tor[j] = NULL;
    pipes_serv_tor[j] = NULL;
  }
      
  // create server to ToR pipes / queues / RlbModules
  for (int j = 0; j < _no_of_nodes; j++) { // sweep nodes
    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
    logfile->addLogger(*queueLogger);

    rlb_modules[j] = alloc_rlb_module(this, j);

    queues_serv_tor[j] = alloc_src_queue(this, queueLogger, j);
    ostringstream oss;
    oss << "NICQueue " << j;
    queues_serv_tor[j]->setName(oss.str());
    //queues_serv_tor[j][k]->setName("Queue-SRC" + ntoa(k + j*_ndl) + "->TOR" +ntoa(j));
    //logfile->writeName(*(queues_serv_tor[j][k]));
    pipes_serv_tor[j] = new Pipe(timeFromNs(delay_host2ToR), *eventlist);
    //pipes_serv_tor[j][k]->setName("Pipe-SRC" + ntoa(k + j*_ndl) + "->TOR" + ntoa(j));
    //logfile->writeName(*(pipes_serv_tor[j][k]));
  }

  // initialize ToR outgoing pipes / queues
  for (int j = 0; j < _ntor; j++) // sweep ToR switches
    for (int k = 0; k < _nul+_ndl; k++) { // sweep ports
      queues_tor[j][k] = NULL;
      pipes_tor[j][k] = NULL;
    }

  for (int j = 0; j < _ntor; j++) { // sweep ToR switches
  // create ToR outgoing pipes / queues
    for (int k = 0; k < _ndl; k++) { // sweep ports
      queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
      logfile->addLogger(*queueLogger);
      queues_tor[j][k] = alloc_queue(queueLogger, _queuesize, j, k);
      ostringstream oss;
      oss << "DLQueue" << j << ":" << k;
      queues_tor[j][k]->setName(oss.str());
      //queues_tor[j][k]->setName("Queue-TOR" + ntoa(j) + "->DST" + ntoa(k + j*_ndl));
      //logfile->writeName(*(queues_tor[j][k]));
      pipes_tor[j][k] = new Pipe(timeFromNs(delay_host2ToR), *eventlist);
      //pipes_tor[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->DST" + ntoa(k + j*_ndl));
      //logfile->writeName(*(pipes_tor[j][k]));
      
    }
  }
  for (int j = 0; j < _ntor; j++) { // sweep ToR switches
    for (int k = 0; k < _adjacency[j].size(); k++) { // sweep ports  
        // it's a link to another ToR
        int port = k+_ndl;
        queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
        logfile->addLogger(*queueLogger);
        queues_tor[j][port] = alloc_queue(queueLogger, _queuesize, j, port);
        ostringstream oss;
        oss << "ULQueue" << j << ":" << port;
        queues_tor[j][port]->setName(oss.str());
        //queues_tor[j][k]->setName("Queue-TOR" + ntoa(j) + "->uplink" + ntoa(k - _ndl));
        //logfile->writeName(*(queues_tor[j][k]));
        pipes_tor[j][port] = new Pipe(timeFromNs(delay_ToR2ToR), *eventlist);
        //pipes_tor[j][k]->setName("Pipe-TOR" + ntoa(j)  + "->uplink" + ntoa(k - _ndl));
        //logfile->writeName(*(pipes_tor[j][k]));
    }
  }
}


int GraphTopology::get_nextToR(int slice, int crtToR, int crtport) {
  int uplink = crtport - _ndl;
  //cout << "Getting next ToR..." << endl;
  //cout << "   uplink = " << uplink << endl;
  //cout << "   next ToR = " << _adjacency[slice][uplink] << endl;
  return _adjacency[crtToR][uplink];
}

int GraphTopology::get_port(int srcToR, int dstToR, int slice, int path_ind, int hop) {
  //cout << "Getting port..." << endl;
  //cout << "   Inputs: srcToR = " << srcToR << ", dstToR = " << dstToR << ", slice = " << slice << ", path_ind = " << path_ind << ", hop = " << hop << endl;
  //cout << "   Port = " << _lbls[srcToR][dstToR][slice][path_ind][hop] << endl;
  return _lbls[srcToR][dstToR][path_ind][hop];
}

int GraphTopology::get_no_paths(int srcToR, int dstToR, int slice) {
  int sz = _lbls[srcToR][dstToR].size();
  return sz;
}

int GraphTopology::get_no_hops(int srcToR, int dstToR, int slice, int path_ind) {
  int sz = _lbls[srcToR][dstToR][path_ind].size();
  return sz;
}
