// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        


#ifndef XPASS_H
#define XPASS_H

/*
 * A XPASS source and sink
 */

#include <list>
#include <map>
#include "config.h"
#include "network.h"
#include "xpasspacket.h"
#include "fairpullqueue.h"
#include "eventlist.h"

#define timeInf 0
#define XPASS_PACKET_SCATTER

//#define LOAD_BALANCED_SCATTER

//min RTO bound in us
// *** don't change this default - override it by calling XPassSrc::setMinRTO()
#define DEFAULT_RTO_MIN 5000

#define RECORD_PATH_LENS // used for debugging which paths lengths packets were trimmed on - mostly useful for BCube

class XPassSink;

class XPassSrc : public PacketSink, public EventSource {
    friend class XPassSink;
 public:
    XPassSrc(EventList &eventlist, int flow_src, int flow_dst);
    uint32_t get_id(){ return id;}
    virtual void connect(Route& routeout, Route& routeback, XPassSink& sink, simtime_picosec startTime);
    void set_traffic_logger(TrafficLogger* pktlogger);
    void startflow();
    void setCwnd(uint32_t cwnd) {_cwnd = cwnd;}
    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}
    void set_flowsize(uint64_t flow_size_in_bytes);
    inline uint64_t get_flowsize() {return _flow_size;} // bytes
    inline void set_start_time(simtime_picosec startTime) {_start_time = startTime;}
    inline simtime_picosec get_start_time() {return _start_time;};

    inline int get_flow_src() {return _flow_src;}
    inline int get_flow_dst() {return _flow_dst;}

    virtual void doNextEvent();
    virtual void receivePacket(Packet& pkt);
    void replace_route(Route* newroute);

    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    void set_paths(vector<const Route*>* rt);

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _new_packets_sent;
    uint64_t _rtx_packets_sent;
    uint64_t _acks_received;
    uint64_t _nacks_received;
    uint64_t _pulls_received;
    uint64_t _implicit_pulls;
    uint64_t _bounces_received;
    uint32_t _cwnd;
    uint64_t _last_acked;
    uint32_t _flight_size;
    uint32_t _acked_packets;
    bool _finished;

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM and
    // PULL_BASED route strategies
    uint16_t _crt_path;
    uint16_t _crt_direction;
    vector<const Route*> _paths;
    vector<const Route*> _original_paths; //paths in original permutation order
    vector<int> _path_counts_new; // only used for debugging, can remove later.
    vector<int> _path_counts_rtx; // only used for debugging, can remove later.
    vector<int> _path_counts_rto; // only used for debugging, can remove later.

    vector <int32_t> _path_acks; //keeps path scores
    vector <int32_t> _path_nacks; //keeps path scores
    vector <bool> _bad_path; //keeps path scores
    vector <int32_t> _avoid_ratio; //keeps path scores
    vector <int32_t> _avoid_score; //keeps path scores

    map<XPassPacket::seq_t, simtime_picosec> _sent_times;
    map<XPassPacket::seq_t, simtime_picosec> _first_sent_times;

    void print_stats();

    int _pull_window; // Used to keep track of expected pulls so we
                      // can handle return-to-sender cleanly.
                      // Increase by one for each Ack/Nack received.
                      // Decrease by one for each Pull received.
                      // Indicates how many pulls we expect to
                      // receive, if all currently sent but not yet
                      // acked/nacked packets are lost
                      // or are returned to sender.
    int _first_window_count;

    //round trip time estimate, needed for coupled congestion control
    simtime_picosec _rtt, _rto, _mdev,_base_rtt;

    uint16_t _mss; // maximum segment size
    uint16_t _pkt_size; // packet size. Equal to _flow_size when _flow_size < _mss. Else equal to _mss
 
    uint32_t _drops;

    XPassSink* _sink;
 
    simtime_picosec _rtx_timeout;
    bool _rtx_timeout_pending;
    const Route* _route;

    const Route *choose_route();

    void pull_packets(XPassPull::seq_t pull_no, XPassPull::seq_t pacer_no);
    void send_packet(XPassPull::seq_t pacer_no);

    virtual const string& nodename() { return _nodename; }
    inline uint32_t flow_id() const { return _flow.flow_id();}
 
    //debugging hack
    void log_me();
    bool _log_me;

    static uint32_t _global_rto_count;  // keep track of the total number of timeouts across all srcs
    static simtime_picosec _min_rto;
    static int _global_node_count;
    static int _rtt_hist[10000000];
    int _node_num;
    PacketFlow _flow;

    int _flow_src; // the sender (source) for this flow
    int _flow_dst; // the receiver (sink) for this flow

 private:
    // Connectivity
    string _nodename;

    enum  FeedbackType {ACK, NACK, BOUNCE, UNKNOWN};
    static const int HIST_LEN=12;
    FeedbackType _feedback_history[HIST_LEN];
    int _feedback_count;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);
    void retransmit_packet();
    void permute_paths();
    void update_rtx_time();
    void process_cumulative_ack(XPassPacket::seq_t cum_ackno);
    inline void count_ack(int32_t path_id) {count_feedback(path_id, ACK);}
    inline void count_nack(int32_t path_id) {count_feedback(path_id, NACK);}
    inline void count_bounce(int32_t path_id) {count_feedback(path_id, BOUNCE);}
    void count_feedback(int32_t path_id, FeedbackType fb);
    bool is_bad_path();
    void log_rtt(simtime_picosec sent_time);
    XPassPull::seq_t _last_pull;
    simtime_picosec _start_time;
    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.
    list <XPassPacket*> _rtx_queue; //Packets queued for (hopefuly) imminent retransmission
};

class XPassPullPacer;

class XPassSink : public PacketSink, public EventSource, public DataReceiver {
    friend class XPassSrc;
 public:
    XPassSink(EventList &eventlist, double w_init, double target_loss);
 

    uint32_t get_id(){ return id;}
    void receivePacket(Packet& pkt);
    void doNextEvent();
    uint32_t _drops;
    uint64_t total_received() const { return _total_received;}
    uint32_t drops(){ return _src->_drops;}
    virtual const string& nodename() { return _nodename; }
    void increase_window() {_pull_no++;} 
    uint64_t cumulative_ack() {return _cumulative_ack;}

    //list<XPassAck::seq_t> _received; // list of packets above a hole, that we've received
 
    XPassSrc* _src;

    //debugging hack
    void log_me();
    bool _log_me;

    void set_paths(vector<const Route*>* rt);

#ifdef RECORD_PATH_LENS
#define MAX_PATH_LEN 20
    vector<uint32_t> _path_lens;
    vector<uint32_t> _trimmed_path_lens;
#endif

    const Route* _route;
 private:
 
    // Connectivity
    void connect(XPassSrc& src, Route& route);
    simtime_picosec nextCreditWait();
    void updateRTT(simtime_picosec ts);
    void feedbackControl();

    inline uint32_t flow_id() const {
	return _src->flow_id();
    };

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM,
    // and PULL_BASED route strategies
    uint16_t _crt_path;
    uint16_t _crt_direction;
    vector<const Route*> _paths; //paths in current permutation order
    vector<const Route*> _original_paths; //paths in original permutation order

   string _nodename;
 
    XPassPullPacer* _pacer;
    XPassPull::seq_t _pull_no; // pull sequence number (local to connection)
    XPassPacket::seq_t _last_packet_seqno; //sequence number of the last
                                         //packet in the connection (or 0 if not known)
    XPassPacket::seq_t _last_packet_pacerno; //sequence number of the last
    uint64_t _cumulative_ack;
    uint64_t _total_received;
    list<uint64_t> _received; // list of packets above a hole, that we've received

    //credit and rate state-keeping
    uint64_t _credit_counter = 0;
    uint64_t _tot_credits = 0;
    uint64_t _drop_credits = 0; 
    double _target_loss;
    double _weight, _max_weight, _min_weight;
    uint64_t _crt_rate, _max_rate, _min_rate; //bps
    bool _is_increasing;
    simtime_picosec _rtt, _max_rtt;
    simtime_picosec _last_fb_update;
    uint64_t _recvd_data;
    simtime_picosec _last_tp_sample_t;
 
    // Mechanism
    void send_ack(simtime_picosec ts, XPassPacket::seq_t ackno, XPassPacket::seq_t pacer_no);
    void send_nack(simtime_picosec ts, XPassPacket::seq_t ackno, XPassPacket::seq_t pacer_no);
    void permute_paths();
    
    int _no_of_paths;
};

class XPassPullPacer : public EventSource {
 public:
    XPassPullPacer(EventList& ev, double pull_rate_modifier);  
    XPassPullPacer(EventList& ev, char* fn);  
    // pull_rate_modifier is the multiplier of link speed used when
    // determining pull rate.  Generally 1 for FatTree, probable 2 for BCube
    // as there are two distinct paths between each node pair.

    void sendPacket(Packet* p, XPassPacket::seq_t pacerno, XPassSink *receiver);
    virtual void doNextEvent();
    uint32_t get_id(){ return id;}
    void enqueue_pulls(XPassSink *receiver);
    void release_pulls(uint32_t flow_id);

    //debugging hack
    void log_me();
    bool _log_me;

    void set_preferred_flow(int id) { _preferred_flow = id;cout << "Preferring flow "<< id << endl;};

 private:
    void set_pacerno(Packet *pkt, XPassPull::seq_t pacer_no);
    FairPullQueue<XPassPull> _pull_queue;
    simtime_picosec _last_pull;
    simtime_picosec _packet_drain_time;
    XPassPull::seq_t _pacer_no; // pull sequence number, shared by all connections on this pacer

    //pull distribution from real life
    static int _pull_spacing_cdf_count;
    static double* _pull_spacing_cdf;

    //debugging
    double _total_excess;
    int _excess_count;
    int _preferred_flow;
};


class XPassRtxTimerScanner : public EventSource {
 public:
    XPassRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerXPass(XPassSrc &tcpsrc);
 private:
    simtime_picosec _scanPeriod;
    typedef list<XPassSrc*> tcps_t;
    tcps_t _tcps;
};

#endif

