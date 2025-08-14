#ifndef GRAPH_TOPO
#define GRAPH_TOPO
#include "dynexp_topology.h"
#include "main.h"
//#include "randomqueue.h"
//#include "pipe.h" // mod
#include "config.h"
#include "loggers.h" // mod
//#include "network.h" // mod
#include "topology.h"
#include "logfile.h" // mod
#include "eventlist.h"
#include <ostream>
#include <vector>

#ifndef QT
#define QT
typedef enum {DEFAULT, COMPOSITE, ECN, INT, CREDIT} queue_type;
#endif

class Queue;
class Pipe;
class Logfile;
class RlbModule;


class GraphTopology: public DynExpTopology {
  public:

  int64_t get_nsuperslice() {return 1;}
  simtime_picosec get_slicetime(int ind) {return 0;} // picoseconds spent in each slice
  int time_to_superslice(simtime_picosec t) {return 0;}
  int time_to_slice(simtime_picosec t) {return 0;} // Given a time, return the slice number (within a cycle)
  int time_to_absolute_slice(simtime_picosec t) {return 0;} // Given a time, return the absolute slice number
  simtime_picosec get_slice_start_time(int slice) {return 0;} // Get the start of a slice

  // defined in source file
  int get_nextToR(int slice, int crtToR, int crtport);
  int get_port(int srcToR, int dstToR, int slice, int path_ind, int hop);
  int get_no_paths(int srcToR, int dstToR, int slice);
  int get_no_hops(int srcToR, int dstToR, int slice, int path_ind);
  int get_nslices() {return 1;} 

  GraphTopology(mem_b queuesize, Logfile* log, EventList* ev, queue_type q, string topfile, 
    map<string,uint64_t> params);
  GraphTopology(mem_b queuesize, Logfile* log, EventList* ev, queue_type q, string topfile);

  void init_network();
  //vector<int>* get_neighbours(int src) {return NULL;};
 protected:
  void read_params(string topfile);
  void set_params();
  // Tor-to-Tor connections across time
  // indexing: [src_node][uplink]->dst_node
  vector<vector<int>> _adjacency;
  map<pair<int,int>, int> _adj_to_port;
  // label switched paths
  // indexing: [src][dst][path_ind][sequence of switch ports (queues)]
  vector<vector<vector<vector<int>>>> _lbls;
  // Connected time slices
};

#endif
