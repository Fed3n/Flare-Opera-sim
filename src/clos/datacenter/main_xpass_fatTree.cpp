// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "config.h"
#include <sstream>
#include <strstream>
#include <fstream> // need to read flows
#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "xpass.h"
#include "creditqueue.h"
#include "firstfit.h"
#include "topology.h"

#include "fat_tree_topology_3to1_k12.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

uint32_t RTT_rack = 50; // ns
uint32_t RTT_net = 500; // ns
int DEFAULT_NODES = 16;

FirstFit* ff = NULL; // not really necessary
//unsigned int subflow_count = 1;

#define DEFAULT_PACKET_SIZE 1500 // full packet (including header), Bytes
#define DEFAULT_HEADER_SIZE 64 // header size, Bytes
#define DEFAULT_QUEUE_SIZE 46


string ntoa(double n);
string itoa(uint64_t n);

EventList eventlist;
Logfile* lg;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    
    Packet::set_packet_size(DEFAULT_PACKET_SIZE - DEFAULT_HEADER_SIZE);
    mem_b queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;
    
    int cwnd = 10;
    stringstream filename(ios_base::out);
    string flowfile; // so we can read the flows from a specified file
    double target_loss = 0.1; //target credit waste ratio
    double winit = 0.5; //initial credit pull rate ratio (w.r.t bandwdith)
    double simtime; // seconds
    double utiltime=1.0; // milliseconds

    int no_of_nodes = DEFAULT_NODES;

    int i = 1;
    filename << "logout.dat";
    RouteStrategy route_strategy = NOT_SET;

    while (i<argc) {
	if (!strcmp(argv[i],"-o")){
	    filename.str(std::string());
	    filename << argv[i+1];
	    i++;
	} else if (!strcmp(argv[i],"-nodes")){
	    no_of_nodes = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-cwnd")){
	    cwnd = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-q")){
	    queuesize = atoi(argv[i+1]) * DEFAULT_PACKET_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-strat")){
	    if (!strcmp(argv[i+1], "perm")) {
			route_strategy = SCATTER_PERMUTE;
	    } else if (!strcmp(argv[i+1], "rand")) {
			route_strategy = SCATTER_RANDOM;
	    } else if (!strcmp(argv[i+1], "pull")) {
			route_strategy = PULL_BASED;
	    } else if (!strcmp(argv[i+1], "single")) {
			route_strategy = SINGLE_PATH;
	    }
	    i++;
	} else if (!strcmp(argv[i],"-flowfile")) {
		flowfile = argv[i+1];
		i++;
    } else if (!strcmp(argv[i],"-winit")) {
        winit = atof(argv[i+1]);
        i++;
    } else if (!strcmp(argv[i],"-tloss")) {
        target_loss = atof(argv[i+1]);
        i++;
    } else if (!strcmp(argv[i],"-simtime")) {
        simtime = atof(argv[i+1]);
        i++;
	} else if (!strcmp(argv[i],"-utiltime")) {
            utiltime = atof(argv[i+1]);
            i++;
    } else 
        exit_error(argv[0]);
      i++;
    }
    srand(13);


    eventlist.setEndtime(timeFromSec(simtime));
    Clock c(timeFromSec(5 / 100.), eventlist);


    if (route_strategy == NOT_SET) {
	fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
	exit(1);
    }

    Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
	cout << "Can't open for writing paths file!"<<endl;
	exit(1);
    }
#endif

    lg = &logfile;




    // !!!!!!!!!!!!!!!!!!!!!!!
    logfile.setStartTime(timeFromSec(10));


    XPassRtxTimerScanner xpassRtxScanner(timeFromMs(1), eventlist);

    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, queuesize, &logfile, &eventlist, ff, CREDIT);

    // initialize all sources/sinks
    XPassSrc::setMinRTO(1000); //increase RTO to avoid spurious retransmits

    //ifstream input("flows.txt");
    ifstream input(flowfile);
    if (input.is_open()){
        string line;
        int64_t temp;
        // get flows. Format: (src) (dst) (bytes) (starttime microseconds)
        while(!input.eof()){
            vector<int64_t> vtemp;
            getline(input, line);
            if(line.length() <= 0) continue;
            stringstream stream(line);
            while (stream >> temp)
                vtemp.push_back(temp);
            //cout << "src = " << vtemp[0] << " dest = " << vtemp[1] << " bytes " << vtemp[2] << " time " << vtemp[3] << endl;
            
            // source and destination hosts for this flow
            int flow_src = vtemp[0];
            int flow_dst = vtemp[1];

            XPassSrc* flowSrc = new XPassSrc(eventlist, flow_src, flow_dst);
            flowSrc->setCwnd(cwnd*Packet::data_packet_size());
            flowSrc->set_flowsize(vtemp[2]); // bytes

            XPassSink* flowSnk = new XPassSink(eventlist, winit, target_loss);
            xpassRtxScanner.registerXPass(*flowSrc);
            Route* routeout, *routein;

            vector<const Route*>* srcpaths = top->get_paths(flow_src, flow_dst);
            int route_idx = 0;
            route_idx = rand()%srcpaths->size(); //ECMP
            routeout = new Route(*(srcpaths->at(route_idx)));
            routeout->push_back(flowSnk);

            vector<const Route*>* dstpaths = top->get_paths(flow_dst, flow_src);
            routein = new Route(*(dstpaths->at(route_idx)));
            routein->push_back(flowSrc);

            flowSrc->connect(*routeout, *routein, *flowSnk, timeFromNs(vtemp[3]/1.));

            flowSrc->set_paths(srcpaths);
            flowSnk->set_paths(dstpaths);
        }
    }


    UtilMonitor* UM = new UtilMonitor(top, eventlist);
    UM->start(timeFromMs(utiltime));

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    //double rtt = timeAsSec(timeFromUs(RTT));
    //logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) { }

}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
