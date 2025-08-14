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
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "xpass.h"
#include "creditqueue.h"
#include "topology.h"

#include "dynexp_topology.h"
#include <list>
#include <map>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

uint32_t delay_host2ToR = 0; // ns
uint32_t delay_ToR2ToR = 500; // ns

#define DEFAULT_PACKET_SIZE 1500 // full packet (including header), Bytes
#define DEFAULT_HEADER_SIZE 64 // header size, Bytes
#define DEFAULT_QUEUE_SIZE 300


string ntoa(double n);
string itoa(uint64_t n);

EventList eventlist;
Logfile* lg;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

map<int,double> read_probfun(string fname){
    map<int,double> hop_to_prob;
    ifstream input(fname);
    if (input.is_open()){
        string line;
        int64_t temp;
        // get flows. Format: (src) (dst) (bytes) (starttime microseconds)
        while(!input.eof()){
            vector<int64_t> vtemp;
            getline(input, line);
            if(line.length() <= 0) continue;
            stringstream stream(line);
            int hops;
            double prob;
            stream >> hops >> prob;
            hop_to_prob[hops] = prob;
            cout << hops << " " << prob << " " << hop_to_prob[hops] << endl;
        }
    } else {
        cout << "Could not open file " << fname << " ! Terminating..." << endl;
        exit(1);
    }
    return hop_to_prob;
}

int main(int argc, char **argv) {
    
    Packet::set_packet_size(DEFAULT_PACKET_SIZE - DEFAULT_HEADER_SIZE);
    mem_b queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;
    mem_b aeolus_thresh = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;
    mem_b cred_queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_HEADER_SIZE;
    mem_b shaping_thresh = DEFAULT_QUEUE_SIZE * DEFAULT_HEADER_SIZE;
    mem_b tent_thresh = 0;
    
    int cwnd = 10;
    stringstream filename(ios_base::out);
    string flowfile; // so we can read the flows from a specified file
    string topfile; // read the topology from a specified file
    double target_loss = 0.1; //target credit waste ratio
    double w_init = 0.5; //initial credit pull rate ratio (w.r.t bandwdith)
    bool fb_sens = false; //weight feedback adjustment with prob function
    bool is_flare = false; //false=flare, true=xpass
    int jit_a = -1; //jittering K value
    int jit_b = -1; //jittering K value
    double fb_w_factor = 2.0; //weight adjustment factor
    double simtime; // seconds
    double utiltime=1.0; // milliseconds
    double tp_sampling= 0.0; //milliseconds
    map<int,double> hops_to_prob = {{1,1.0},{2,1.0},{3,1.0},{4,1.0},{5,1.0}};

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
	if (!strcmp(argv[i],"-o")){
	    filename.str(std::string());
	    filename << argv[i+1];
	    i++;
	} else if (!strcmp(argv[i],"-cwnd")){
	    cwnd = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-q")){
	    queuesize = atoi(argv[i+1]) * DEFAULT_PACKET_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-credq")){
	    cred_queuesize = atoi(argv[i+1]) * DEFAULT_HEADER_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-qshaping")){
	    shaping_thresh = atoi(argv[i+1]) * DEFAULT_HEADER_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-aeolus")){
	    aeolus_thresh = atoi(argv[i+1]) * DEFAULT_PACKET_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-tent")){
	    tent_thresh = atoi(argv[i+1]) * DEFAULT_HEADER_SIZE;
	    i++;
	} else if (!strcmp(argv[i],"-fbw")){
	    fb_w_factor = atof(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-jita")){
	    jit_a = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-jitb")){
	    jit_b = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-fbsens")){
	    fb_sens = true;
	} else if (!strcmp(argv[i],"-flare")){
	    is_flare = true;
	} else if (!strcmp(argv[i],"-probfile")) {
		string probfile = argv[i+1];
        hops_to_prob = read_probfun(probfile);
		i++;
	} else if (!strcmp(argv[i],"-flowfile")) {
		flowfile = argv[i+1];
		i++;
  } else if (!strcmp(argv[i],"-topfile")) {
      topfile = argv[i+1];
      i++;
    } else if (!strcmp(argv[i],"-winit")) {
        w_init = atof(argv[i+1]);
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
	} else if (!strcmp(argv[i],"-tp")) {
            tp_sampling = atof(argv[i+1]);
            i++;
    } else 
        exit_error(argv[0]);
      i++;
    }
    fast_srand(13);
    srand(13);


    eventlist.setEndtime(timeFromSec(simtime));
    Clock c(timeFromSec(5 / 100.), eventlist);

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

    map<string,uint64_t> params = 
        {{"cq_size",cred_queuesize},{"sh_thresh",shaping_thresh},
        {"ae_thresh",aeolus_thresh},{"te_thresh",tent_thresh}};
    DynExpTopology* top = new DynExpTopology(queuesize, &logfile, &eventlist, 
        CREDIT, topfile, params);
    top->set_prob_hops(hops_to_prob);

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

            XPassSrc* flowSrc = new XPassSrc(eventlist, flow_src, flow_dst, is_flare, top);
            flowSrc->setCwnd(cwnd*Packet::data_packet_size());
            flowSrc->set_flowsize(vtemp[2]); // bytes

            XPassSink* flowSnk = new XPassSink(eventlist);
            flowSnk->set_w_init(w_init);
            flowSnk->set_target_loss(target_loss);
            flowSnk->set_w_init(w_init);
            flowSnk->set_fb_w_factor(fb_w_factor);
            flowSnk->set_fb_sens(fb_sens);
            flowSnk->set_jit_alpha(jit_a);
            flowSnk->set_jit_beta(jit_b);
            flowSnk->set_tp_sampling(timeFromMs(tp_sampling));
            xpassRtxScanner.registerXPass(*flowSrc);

            flowSrc->connect(*flowSnk, timeFromNs(vtemp[3]/1.));

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
