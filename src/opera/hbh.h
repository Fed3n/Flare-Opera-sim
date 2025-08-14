#ifndef HBH_H
#define HBH_H

#include "config.h"
#include "datacenter/dynexp_topology.h"
#include "network.h"
#include "hbhpacket.h"
#include "eventlist.h"

#define timeInf 0
/* A HBH source and sink */

class HbHSrc : public PacketSink, public EventSource {
    friend class HbHSink;
 public:
    HbHSrc(EventList &eventlist, int flow_src, int flow_dst, DynExpTopology *top);
    uint32_t get_id(){ return id;}
    virtual void connect(HbHSink& sink, simtime_picosec startTime);
    void set_traffic_logger(TrafficLogger* pktlogger);
    void startflow();
    void setCwnd(uint32_t cwnd) {_cwnd = cwnd; _init_cwnd = cwnd;}
    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}
    void set_flowsize(uint64_t flow_size_in_bytes);
    inline uint64_t get_flowsize() {return _flow_size;} // bytes
    inline void set_start_time(simtime_picosec startTime) {_start_time = startTime;}
    inline simtime_picosec get_start_time() {return _start_time;};
    void addToLost(HbHPacket *pkt);
    void setFinished();

    inline int get_flow_src() {return _flow_src;}
    inline int get_flow_dst() {return _flow_dst;}

    virtual void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _new_packets_sent;
    uint64_t _rtx_packets_sent;
    uint32_t _max_NIC_pkts;
    uint32_t _cwnd, _init_cwnd;
    uint64_t _last_acked;
    uint32_t _flight_size, _max_inflight;
    uint32_t _acked_packets;
    bool _finished;
    DynExpTopology *_top;
    uint64_t _flow_id;

    //round trip time estimate, needed for coupled congestion control
    simtime_picosec _rtt, _rto, _mdev,_base_rtt;

    uint16_t _mss; // maximum segment size
    uint16_t _pkt_size; // packet size. Equal to _flow_size when _flow_size < _mss. Else equal to _mss
 
    uint32_t _drops;

    HbHSink* _sink;
 
    simtime_picosec _rtx_timeout;
    bool _rtx_timeout_pending;

    void send_packet();
    void triggerSending();

    virtual const string& nodename() { return _nodename; }
    inline uint32_t flow_id() const { return _flow.flow_id();}
 
    //debugging hack
    void log_me();
    bool _log_me;

    static uint32_t _global_rto_count;  // keep track of the total number of timeouts across all srcs
    static simtime_picosec _min_rto;
    int _node_num;
    PacketFlow _flow;

    int _flow_src; // the sender (source) for this flow
    int _flow_dst; // the receiver (sink) for this flow

 private:
    // Connectivity
    string _nodename;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);
    void retransmit_packet();
    void update_rtx_time();
    void process_cumulative_ack(HbHPacket::seq_t cum_ackno);
    Queue* sendToNIC(Packet* pkt);
    simtime_picosec _start_time;
    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.
    list <HbHPacket*> _rtx_queue; //Packets queued for (hopefuly) imminent retransmission
};

class HbHSink : public PacketSink, public DataReceiver, public Logged {
    friend class HbHSrc;
 public:
    HbHSink();
    void receivePacket(Packet& pkt);
    uint32_t _cumulative_ack; // the packet we have cumulatively acked
    uint32_t drops(){ return _src->_drops;}
    uint32_t get_id(){ return id;}
    uint32_t flow_id() { return _src->flow_id(); }
    uint64_t cumulative_ack(){ return _cumulative_ack;}
    virtual const string& nodename() { return _nodename; }

    list<pair<uint64_t,uint64_t>> _received; // list of packets above a hole, <seqno,segsize>

    HbHSrc* _src;
 private:
    // Connectivity
    uint16_t _crt_path;
    simtime_picosec last_ts = 0;
    unsigned last_hops = 0;
    unsigned last_queueing = 0;
    unsigned last_seqno = 0;
    unsigned _total_hops = 0;
    unsigned _last_packet_seqno = 0;
    uint64_t _total_received = 0;


    void connect(HbHSrc& src);
    //const Route* _route;

    string _nodename;
};

#endif
