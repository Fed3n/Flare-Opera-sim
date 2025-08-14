// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "dctcp.h"
#include "ecn.h"
//#include "mtcp.h"
#include "config.h"
#include "loggertypes.h"

string ntoa(double n);
extern unsigned total_flows;


////////////////////////////////////////////////////////////////
//  DCTCP SOURCE
////////////////////////////////////////////////////////////////

DCTCPSrc::DCTCPSrc(TcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, 
        DynExpTopology *top, int flow_src, int flow_dst, bool tdtcp) 
    : TcpSrc(logger, pktlogger, eventlist, top, flow_src, flow_dst, tdtcp)
{
#ifdef TDTCP
/*
    _past_cwnd.resize(_top->get_nsuperslice());
    for(int i = 0; i < _past_cwnd.size(); i++)
        _past_cwnd[i] = 2*Packet::data_packet_size();
    _alfa.resize(_top->get_nsuperslice());
    for(int i = 0; i < _alfa.size(); i++)
        _alfa[i] = 0;
    _pkts_seen.resize(_top->get_nsuperslice());
    for(int i = 0; i < _pkts_seen.size(); i++)
        _pkts_seen[i] = 0;
    _pkts_marked.resize(_top->get_nsuperslice());
    for(int i = 0; i < _pkts_marked.size(); i++)
        _pkts_marked[i] = 0;
*/
#else
    _pkts_seen = 0;
    _pkts_marked = 0;
    _past_cwnd = 2*Packet::data_packet_size();
    _alfa = 0;
#endif
    _rto = timeFromMs(0.2);    
}

void 
DCTCPSrc::startflow() {
#ifdef TDTCP
    _pkts_seen.resize(_top->get_nsuperslice());
    for(size_t i = 0; i < _pkts_seen.size(); i++)
        _pkts_seen[i] = 0;
    _pkts_marked.resize(_top->get_nsuperslice());
    for(size_t i = 0; i < _pkts_marked.size(); i++)
        _pkts_marked[i] = 0;
    _past_cwnd.resize(_top->get_nsuperslice());
    for(size_t i = 0; i < _past_cwnd.size(); i++)
        _past_cwnd[i] = 2*Packet::data_packet_size();
    _alfa.resize(_top->get_nsuperslice());
    for(size_t i = 0; i < _alfa.size(); i++)
        _alfa[i] = 0;
#endif
    TcpSrc::startflow();
}

void
DCTCPSrc::cleanup() {
#ifdef TDTCP
    _past_cwnd.clear();
    _alfa.clear();
    _pkts_seen.clear();
    _pkts_marked.clear();
#endif
    return;
}

//drop detected
#ifdef TDTCP
void
DCTCPSrc::deflate_window(int slice){
    _pkts_seen[tcp_slice(slice)] = 0;
    _pkts_marked[tcp_slice(slice)] = 0;
	_ssthresh[tcp_slice(slice)] = max(_cwnd[tcp_slice(slice)]/2, (uint32_t)(2 * _mss));

    _past_cwnd[tcp_slice(slice)] = _cwnd[tcp_slice(slice)];
}
#else
void
DCTCPSrc::deflate_window(){
    _pkts_seen = 0;
    _pkts_marked = 0;
	_ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));

    _past_cwnd = _cwnd;
}
#endif


void
DCTCPSrc::receivePacket(Packet& pkt) {
    TcpAck *p = (TcpAck*)(&pkt);
    int slice = p->get_tcp_slice();
    //TEST don't do anything with the packet
    if(pkt.early_fb()) {
        return TcpSrc::receivePacket(pkt);
    }
    if(_finished) {
        cleanup();
        return TcpSrc::receivePacket(pkt);
    }
#ifdef TDTCP
    _pkts_seen[tcp_slice(slice)]++;

    if (pkt.flags() & ECN_ECHO){
        _pkts_marked[tcp_slice(slice)] += 1;
        //cout << "ECN mark " << _flow_id << endl;

        //exit slow start since we're causing congestion
        if (_ssthresh[tcp_slice(slice)]>_cwnd[tcp_slice(slice)])
            _ssthresh[tcp_slice(slice)] = _cwnd[tcp_slice(slice)];
    }

    if (_pkts_seen[tcp_slice(slice)] * _mss >= _past_cwnd[tcp_slice(slice)]){
        double f = (double)_pkts_marked[tcp_slice(slice)]/_pkts_seen[tcp_slice(slice)];
        _alfa[tcp_slice(slice)] = 15.0/16.0 * _alfa[tcp_slice(slice)] + 1.0/16.0 * f;
        _pkts_seen[tcp_slice(slice)] = 0;
        int marked = _pkts_marked[tcp_slice(slice)];
        _pkts_marked[tcp_slice(slice)] = 0;

        if (_alfa[tcp_slice(slice)]>0){
            if(marked > 0) {
                //cout << "ECN triggered at " << _flow_id << " at slice " << slice << " marked " << marked << endl;
              }
                _cwnd[tcp_slice(slice)] = _cwnd[tcp_slice(slice)] * (1-_alfa[tcp_slice(slice)]/2);

            if (_cwnd[tcp_slice(slice)]<_mss)
                _cwnd[tcp_slice(slice)] = _mss;
            else if (_cwnd[tcp_slice(slice)] > get_max_cwnd())
                _cwnd[tcp_slice(slice)] = get_max_cwnd();

            _ssthresh[tcp_slice(slice)] = _cwnd[tcp_slice(slice)];
        }
        _past_cwnd[tcp_slice(slice)] = _cwnd[tcp_slice(slice)];
        //	cout << ntoa(timeAsMs(eventlist().now())) << " ID " << str() << " PKTS MARKED " << _pkts_marks;
#else
        _pkts_seen++;

        if (pkt.flags() & ECN_ECHO){
            _pkts_marked += 1;
            //cout << "ECN mark " << _flow_id << endl;

            //exit slow start since we're causing congestion
            if (_ssthresh>_cwnd)
                _ssthresh = _cwnd;
        }

        if (_pkts_seen * _mss >= _past_cwnd){
            //update window, once per RTT
            double f = (double)_pkts_marked/_pkts_seen;
            //	cout << ntoa(timeAsMs(eventlist().now())) << " ID " << str() << " PKTS MARKED " << _pkts_marks;
            _alfa = 15.0/16.0 * _alfa + 1.0/16.0 * f;
            _pkts_seen = 0;
            int marked = _pkts_marked;
            _pkts_marked = 0;
            if (_alfa>0){
                if(marked > 0){
                    //cout << "ECN triggered at " << _flow_id << " at slice " << slice << " marked " << marked << endl;
                }
                _cwnd = _cwnd * (1-_alfa/2);

                if (_cwnd<_mss)
                    _cwnd = _mss;
                else if (_cwnd > get_max_cwnd())
                    _cwnd = get_max_cwnd();

                _ssthresh = _cwnd;
            }
            _past_cwnd = _cwnd;
#endif

            //cout << ntoa(timeAsMs(eventlist().now())) << " UPDATE " << str() << " CWND " << _cwnd << " alfa " << ntoa(_alfa)<< " marked " << ntoa(f) << endl;
            /*
               if(get_flowid() == 0){
               cout << "CRTCWND " << _cwnd << " " << eventlist().now() << " " << total_flows << endl;
               }
             */
        }

        TcpSrc::receivePacket(pkt);
        //cout << ntoa(timeAsMs(eventlist().now())) << " ATCPID " << str() << " CWND " << _cwnd << " alfa " << ntoa(_alfa)<< endl;
    }

void 
DCTCPSrc::rtx_timer_hook(simtime_picosec now,simtime_picosec period){
    TcpSrc::rtx_timer_hook(now,period);
};

void DCTCPSrc::doNextEvent() {
    if(!_rtx_timeout_pending) {
        startflow();
    }
    TcpSrc::doNextEvent();
}

