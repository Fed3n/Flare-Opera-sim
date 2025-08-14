#include "hbhqueue.h"
#include "config.h"
#include "datacenter/dynexp_topology.h"
#include "network.h"
#include "hbh.h"
#include "hbhpacket.h"
#include "pipe.h"
#include <cmath>
#include <iostream>
#include <math.h>
#include <algorithm>
#include <list>
#include <vector>

bool debug_queue(int tor, int port) {
    return false;
    if(tor == 41 && port == 6) return true;
}

void 
HbHQueue::enqueuePacket(Packet &pkt) {
    assert(pkt.type() == HBHDATA);
    HbHPacket *hbhp = (HbHPacket*)(&pkt);
    uint32_t seqno = hbhp->seqno();
    if(_sorted_enqueue) {
        //insert ordered based on seqno
        list<Packet*>::iterator it;
        for(it = _enqueued.begin(); it != _enqueued.end(); ++it) {
            HbHPacket *p = (HbHPacket*)(*it);
            if(seqno >= p->seqno()) {
                //insert before greater/equal seqno
                _enqueued.insert(it, &pkt);
                return;
            }
        }
        //we are the smallest seqno, insert to back
        _enqueued.push_back(&pkt);
    } else {
        //just push the packet into the queue
        _enqueued.push_front(&pkt);
    }
}

//TODO
//check for potentially stuck packets when sending at link down
//what if nextq is NULL? maybe set timer to prevent getting stuck?
//optimize amount of polling to minimum
//why is queue size so large

HbHQueue::HbHQueue(linkspeed_bps bitrate, mem_b maxsize, 
                     EventList& eventlist, QueueLogger* logger,
                     int tor, int port, DynExpTopology *top,
                     unsigned D, unsigned P)
: Queue(bitrate,maxsize,eventlist,logger,tor,port,top)
{
    _next_tx_pkt = NULL;
    _servicing = false;
    _is_NIC = false;
    _is_timer_set = false;
    _D = D;
    _P = P;
    _is_downlink = _is_NIC ? false : port < top->no_of_hpr();
    if(_is_downlink) _D = 300;
    _sorted_enqueue = false;
}

bool
HbHQueue::is_queue_connected(Queue *q) {
    HbHQueue *hbhq = (HbHQueue*)q;
    if(hbhq->_is_NIC) return true;
    assert(!hbhq->_is_downlink);
    int tor = q->_tor;
    int port = q->_port;
    int slice = _top->time_to_slice(eventlist().now());
    int adjToR = _top->get_nextToR(slice, _tor, port);
    return adjToR == tor;
}

int
HbHQueue::getNextPort(Packet &pkt) {
    if(pkt.id() == 57) {
        //cout << "PKT NEXTPORT NIC?" << _is_NIC << " " << pkt.get_src_ToR() << " " << _top->get_firstToR(pkt.get_dst()) << " " << pkt.get_slice_sent() << " " << pkt.get_path_index() << " " << pkt.get_crthop()+1 << endl;
    }
    if(_is_NIC) {
        //if we are in the NIC, route is to be set and depends on current time slice
        //assumes single-path routing (how would this even work w/ multipathing?)
        //cout << pkt.get_src() << " " << pkt.get_dst() << endl;
        int slice = _top->time_to_slice(eventlist().now());
        return _top->get_port(_top->get_firstToR(pkt.get_src()), _top->get_firstToR(pkt.get_dst()),
                slice, 0, 0);
    } else {
        int next_hop = pkt.get_crthop() + 1;
        //if it's the last hop, downlink port is fixed
        if(next_hop == pkt.get_maxhops()) {
            return _top->get_lastport(pkt.get_dst()); 
        } else {
            //if we are in the fabric, route is already set (opera source routing)
            return _top->get_port(pkt.get_src_ToR(), _top->get_firstToR(pkt.get_dst()),
                    pkt.get_slice_sent(), pkt.get_path_index(), next_hop);
        }
    }
}

int
HbHQueue::getNextNextPort(Packet &pkt) {
    if(pkt.id() == 57) {
        //cout << "PKT NEXTNEXTPORT NIC?" << _is_NIC << " " << pkt.get_src_ToR() << " " << _top->get_firstToR(pkt.get_dst()) << " " << pkt.get_slice_sent() << " " << pkt.get_path_index() << " " << pkt.get_crthop()+2 << endl;
    }
    if(_is_NIC) {
        int slice = _top->time_to_slice(eventlist().now());
        //if it's the last hop, downlink port is fixed
        int max_hops = _top->get_no_hops(_top->get_firstToR(pkt.get_src()), _top->get_firstToR(pkt.get_dst()), slice, 0);
        if(max_hops == 1) {
            return _top->get_lastport(pkt.get_dst()); 
        } else {
            return _top->get_port(_top->get_firstToR(pkt.get_src()), _top->get_firstToR(pkt.get_dst()),
                    slice, 0, 1);
        }
    } else {
        int nextPort = getNextPort(pkt);
        if(nextPort < _top->no_of_hpr()) return -1; //downlink has no next port
        int next_hop = pkt.get_crthop() + 2;
        assert(pkt.get_crthop() <= pkt.get_maxhops());
        //if it's the last hop, downlink port is fixed
        if(next_hop == pkt.get_maxhops()) {
            return _top->get_lastport(pkt.get_dst()); 
        } else {
            //if we are in the fabric, route is already set (opera source routing)
            return _top->get_port(pkt.get_src_ToR(), _top->get_firstToR(pkt.get_dst()),
                    pkt.get_slice_sent(), pkt.get_path_index(), next_hop);
        }
    }
}

//if no valid nextQ (link down) returns NULL
Queue*
HbHQueue::getNextQueue(Packet &pkt) {
    int slice = _top->time_to_slice(eventlist().now());
    int nextToR;
    //if NIC, first hop ToR is static. otherwise, based on current timeslice
    if(_is_NIC) {
        //cout << "getNextQueue NIC src " << pkt.get_src() << endl;
        nextToR = _top->get_firstToR(pkt.get_src());
    } else {
        //cout << "getNextQueue slice " << slice << " " << _tor << " " << _port << endl;
        nextToR = _top->get_nextToR(slice, _tor, _port);
    }
    //cout << " getNextQueue nextToR " << nextToR << " getNextPort " << getNextPort(pkt) << endl;
    if(nextToR < 0) {
        return NULL;
    }
    Queue *q = _top->get_queue_tor(nextToR, getNextPort(pkt));
    //cout << nextToR << " " << getNextPort(pkt) << endl;
    assert(q); 
    return q;
}

Queue*
HbHQueue::getNextQueue(int port) {
    int slice = _top->time_to_slice(eventlist().now());
    int nextToR = _top->get_nextToR(slice, _tor, _port);
    if(nextToR < 0) {
        return NULL;
    }
    Queue *q = _top->get_queue_tor(nextToR, port);
    assert(q); 
    return q;
}

//XXX always check this if there are bugs
void 
HbHQueue::packetWasDropped(Packet& pkt) {
    assert(_dst_to_queued[pkt.get_dst()/6] > 0);
    _dst_to_queued[pkt.get_dst()/6]--;
    assert(_flowid_to_queued[pkt.flow_id()] > 0);
    _flowid_to_queued[pkt.flow_id()]--;
    if(!_is_downlink) {
        assert(_port_to_queued[getNextNextPort(pkt)] > 0);
        _port_to_queued[getNextNextPort(pkt)]--;
    }
    //assert(_hops_to_queued[pkt.get_crthop()] > 0);
    //_hops_to_queued[pkt.get_crthop()]--;
    //_flowid_to_queued[pkt.flow_id()]--;
}

void
HbHQueue::pollme(Queue* me) {
    //are we already in queue? 
    bool in_waiting = (std::find(_queues_waiting.begin(), _queues_waiting.end(), me) 
            != _queues_waiting.end());
    if(!in_waiting) {
        _queues_waiting.push_back(me);
    }
    return poll();
    //if(!_is_NIC && _queuesize/1500.0 >= _Q) return; //max queuesize invariant
    bool extracted = me->extractNext(_port, _flowid_to_queued, 
        _dst_to_queued, _port_to_queued);
    if(!extracted) {
        //are we already in queue? 
        bool in_waiting = (std::find(_queues_waiting.begin(), _queues_waiting.end(), me) 
                != _queues_waiting.end());
        if(!in_waiting) {
            _queues_waiting.push_back(me);
        }
    }
}

void
HbHQueue::poll() {
    list<Queue*>::iterator it;
    it = _queues_waiting.begin();
    while(it != _queues_waiting.end()) {
        Queue* q = (*it);
        if(!is_queue_connected(q)) {
            it = _queues_waiting.erase(it);
            continue; 
        }
        if(!q->is_servicing()) {
            bool extracted = q->extractNext(_port, _flowid_to_queued, 
                _dst_to_queued, _port_to_queued);
            if(extracted) {
                it = _queues_waiting.erase(it);
                continue;
            }
        }
        ++it;
    }
}

//try to extract packet from sending queue while keeping queueing invariant for receiveing queue
//if any found, begin servicing it and return true. else false
//assumes a PIEO queueing model where we can extract from any queue position in O(1)
bool
HbHQueue::extractNext(int port, map<uint64_t, unsigned> &flowid_to_queued, map<int, unsigned> &dst_to_queued, map<int, unsigned> &port_to_queued) {
    //cout << nodename() << "extractNext" << endl;
    int slice = _top->time_to_slice(eventlist().now());
    std::list<Packet*>::reverse_iterator rit; 
    for(rit = _enqueued.rbegin(); rit != _enqueued.rend(); ++rit) {
        Packet *pkt = *rit;
        uint64_t flowid = pkt->flow_id();
        int dst = pkt->get_dst()/6;
        int pkt_port = getNextPort(*pkt);
        int nxt_pkt_port = getNextNextPort(*pkt); //-1 if pkt_port is downlink
        //if(pkt->id() == 255024 || debug_queue(_tor, _port))
        if(pkt->id() == 255024)
        cout << nodename() << "extractNext crthop " << pkt->get_crthop() << " dst " << pkt->get_dst()/6 << " ports " << port << " " << pkt_port << " invariants " << dst_to_queued[dst/6] << " " << port_to_queued[nxt_pkt_port] << " " << eventlist().now() << endl;
        //if pkt is addressed to queue port and maintains invariant, extract it and service it
        //downlinks do not need to keep the invariant
        if(port == pkt_port && ((nxt_pkt_port == -1) || (dst_to_queued[dst] <= _D && port_to_queued[nxt_pkt_port] <= _P))) {
            //cout << nodename() << "extractNext success invariants " << _D << " " << _P << endl;
            _next_tx_pkt = pkt;
            _enqueued.erase(std::next(rit).base()); //already remove from queue, but queuesize gets changed later in completeService()
            assert(!_servicing);
            if(_is_NIC) {
                setupNICPkt(_next_tx_pkt);
            }
            Queue *nextq = getNextQueue(*pkt);
            assert(nextq);
            _next_tx_pkt->set_nextq(nextq);
            //if(pkt->flow_id() == 22 && _next_tx_pkt->nextq()->_tor == 17 && _next_tx_pkt->nextq()->_port == 7) cout << "FLOW EXTRACTNEXT " << _next_tx_pkt->id() << endl;
            dst_to_queued[dst]++;
            flowid_to_queued[flowid]++;
            if(nxt_pkt_port >= 0) {
                port_to_queued[nxt_pkt_port]++;
            }
            if(nextq->_tor == 83 && nextq->_port == 0 && flowid == 478) {
                //cout << "PAKT EXTRACTNEXT " << pkt->id() << " " << flowid_to_queued[flowid] << endl;
            }
            if(pkt->id() == 57) {
                //cout << "PKT EXTRACTNEXT " << pkt->flow_id() << " " << nextq->_tor << " " << nextq->_port << " " << slice << " " << pkt_port << " " << nxt_pkt_port << endl;
            }
            //XXX if it's a NIC packet, we must setup route already here. optimal route may have changed by the time completeService() is called. (i.e., in 120ns)
            //otherwise, the packet may enter a queue it does not have a token for and cause unspeakable things (esp. i.r.t. state keeping)
            beginService();
            return true;
        }
    }
    return false;
}

void
HbHQueue::receivePacket(Packet & pkt) {
    assert(pkt.type() == HBHDATA);
    HbHPacket *hbhp = (HbHPacket*)(&pkt);
    HbHSrc *hbhsrc = hbhp->get_hbhsrc();
    if(pkt.id() == 255024 || debug_queue(_tor, _port))
    cout << nodename() << " receivePacket " << pkt.flow_id() << " seqno " << hbhp->seqno() << " crtport " 
      << pkt.get_crtport() << " queuesize " << _queuesize << " servicing? " << _servicing << " " << eventlist().now() << endl;
    if(pkt.id() == 57) {
        //cout << "PKT " << pkt.flow_id() << " " << _flowid_to_queued[pkt.flow_id()] << " " << pkt.get_crtToR() << endl;
    }
    //cout << "DL?" << _is_downlink << endl;
    if (queuesize()+pkt.size() > _maxsize) {
        cout << nodename() << " DROPPED" << endl;
        assert(_dst_to_queued[pkt.get_dst()/6] > 0);
        _dst_to_queued[pkt.get_dst()/6]--;
        assert(_flowid_to_queued[pkt.flow_id()] > 0);
        _flowid_to_queued[pkt.flow_id()]--;
        if(!_is_downlink) {
            assert(_port_to_queued[getNextPort(pkt)] > 0);
            _port_to_queued[getNextPort(pkt)]--;
        }
        if(pkt.type() == HBHDATA){
            hbhsrc->addToLost(hbhp);
        } else {
            pkt.free();
        }
        _top->inc_losses();
        _num_drops++;
        return;
    }
    
    if(!_is_downlink) {
        Queue *nextq = getNextQueue(pkt);
        if(nextq == NULL) {
            /*
               int abs_slice = _top->time_to_absolute_slice(eventlist().now());
               simtime_picosec next_slice_t = _top->get_slice_start_time(abs_slice+1);
               eventlist().sourceIsPending(*this, next_slice_t);
               _is_timer_set = true;
               return; //likely missed next connection.
               */
            //next connection already down, best thing is to let the packet go
            //best case scenario otherwise it would have had to wait for the next cycle, i.e., be queued for several ms
            cout << nodename() << "DROP (missed connection)" << endl;
            assert(_dst_to_queued[pkt.get_dst()/6] > 0);
            _dst_to_queued[pkt.get_dst()/6]--;
            assert(_flowid_to_queued[pkt.flow_id()] > 0);
            _flowid_to_queued[pkt.flow_id()]--;
            assert(_port_to_queued[getNextPort(pkt)] > 0);
            _port_to_queued[getNextPort(pkt)]--;
            assert(pkt.type() == HBHDATA);
            hbhsrc->addToLost(hbhp);
            return;
        }
    }

    enqueuePacket(pkt);
    _queuesize += pkt.size();
    pkt.inc_queueing(_queuesize);
    pkt.set_last_queueing(_queuesize);
    
    /*
    _flowid_to_queued[pkt.flow_id()]++;
    _port_to_queued[getNextPort(pkt)]++;
    _hops_to_queued[pkt.get_crthop()]++;
    */
       
    //record queuesize per slice
    int slice = _top->time_to_superslice(eventlist().now());
    if (queuesize() > _max_recorded_size_slice[slice]) {
        _max_recorded_size_slice[slice] = queuesize();
    }
    if (queuesize() > _max_recorded_size) {
        _max_recorded_size = queuesize();
    }
    //downlink packets do not need any token to get to their destination
    if(_is_downlink && !_servicing) {
        _next_tx_pkt = _enqueued.back();
        _enqueued.pop_back();
        beginService();
        return;
    }
    //otherwise, uplink needs the next packet pulled, if possible
    //if not already servicing packet, check if next queue for packet can receive it
    if(!_servicing) {
        Queue *nextq = getNextQueue(pkt);
        //cout << "receivePacket\n";
        //nextq->poll();
        nextq->pollme(this);
    }
}

void 
HbHQueue::beginService() {
    if(_next_tx_pkt->id() == 255024 || debug_queue(_tor, _port))
    cout << nodename() << " beginService " << " queuesize " << _queuesize << " " << eventlist().now() << endl;
    //if(_is_NIC && _next_tx_pkt->id() == 4714) cout << "NIC BEGINSERVICE" << endl;
    /* schedule the next dequeue event */
    //at this point in HbHQueue packet arrived w/ extractNext will have been removed from queue
    assert(!_servicing);
    assert(_next_tx_pkt != NULL);
    if(_next_tx_pkt->id() == 57) {
        //cout << "PKT " << _next_tx_pkt->flow_id() << " " << _flowid_to_queued[_next_tx_pkt->flow_id()] << " " << _next_tx_pkt->get_crtToR() << endl;
    }
    eventlist().sourceIsPendingRel(*this, drainTime(_next_tx_pkt));
    _servicing = true;
}

void 
HbHQueue::completeService() {
    if(_next_tx_pkt->id() == 255024 || debug_queue(_tor, _port))
    cout << nodename() << " completeService pktid " << _next_tx_pkt->id() << " flowid " << _next_tx_pkt->flow_id() << " port " << getNextPort(*_next_tx_pkt) << " queuesize " << _queuesize << " " << eventlist().now() << endl;
    static map<uint64_t, unsigned> test_pkt_cnt;
    if(_next_tx_pkt->flow_id() == 22 && _tor == 17 && _port == 7) {
        test_pkt_cnt[_next_tx_pkt->id()]++;
        assert(test_pkt_cnt[_next_tx_pkt->id()] == 1);
    }
    //if(_next_tx_pkt->flow_id() == 22 && _tor == 17 && _port == 7) cout << "FLOW COMPLETESERVICE " << _next_tx_pkt->id() << endl;
    /* dequeue the packet selected from extractNext() */
    assert(_next_tx_pkt != NULL);
    Packet* pkt = _next_tx_pkt;
    //cout << _next_tx_pkt->flow_id() << " " << _next_tx_pkt->id() << endl;
    //if(pkt->id() == 57) cout << "PKT " << pkt->id() << " " << _flowid_to_queued[pkt->flow_id()] << " " << getNextPort(*pkt) << endl;
    if(_tor == 83 && _port == 0 && _next_tx_pkt->flow_id() == 478) {
        //cout << "PAKT COMPLETESERVICE " << pkt->id() << " " << _flowid_to_queued[pkt->flow_id()] << endl;
    }
    assert(_dst_to_queued[pkt->get_dst()/6] > 0);
    _dst_to_queued[pkt->get_dst()/6]--;
    assert(_flowid_to_queued[pkt->flow_id()] > 0);
    _flowid_to_queued[pkt->flow_id()]--;
    if(!_is_downlink) {
        assert(_port_to_queued[getNextPort(*pkt)] > 0);
        _port_to_queued[getNextPort(*pkt)]--;
    }
    //assert(_hops_to_queued[pkt->get_crthop()] > 0);
    //_hops_to_queued[pkt->get_crthop()]--;
    assert(_queuesize >= pkt->size());
    _queuesize -= pkt->size();

    /* tell the packet to move on to the next pipe */
    sendFromQueue(pkt);
    _servicing = false;

    //downlink packets do not need any token to get to their destination
    if(_is_downlink && !_enqueued.empty()) {
        //downlink packets need to be removed from queue
        _next_tx_pkt = _enqueued.back();
        _enqueued.pop_back();
        beginService(); 
        return;
    }
    //otherwise, uplink needs the next packet pulled, if possible
    if (!_enqueued.empty()) {
        assert(!_servicing);
        //check if any receive queue can take another packet
        /*
        map<int, unsigned>::iterator mit;
        for(mit = _port_to_queued.begin(); mit != _port_to_queued.end(); ++mit){
            int port = mit->first;
            unsigned queued = mit->second;
            if(queued > 0) {
                Queue *q = getNextQueue(port);
                if(q == NULL) continue;
                q->pollme(this);
                //if polling successful, we called beginService()
                if(_servicing) break; 
            }
        }
        */
        list<Packet*>::reverse_iterator rit;
        for(rit = _enqueued.rbegin(); rit != _enqueued.rend(); ++rit) {
            Queue *q = getNextQueue(*(*rit));
            if(q == NULL) continue;
            q->pollme(this);
            //if beginService() got called, we stop checking
            if(_servicing) break; 
        }
    }
    poll();
}

void
HbHQueue::doNextEvent() {
    //if servicing it should be the completeService callback
    if(_servicing) {
        completeService();
    } else {
    //else it should be the wake-up timer to poll queues after new circuit is up
        assert(_is_timer_set);
        _is_timer_set = false;
        //cout << "doNextEvent\n";
        poll();
    }
}

void 
HbHQueue::reportMaxqueuesize() {
    simtime_picosec crt_t = eventlist().now();
    cout << "Queue " << _tor << " " << _port << " " << _max_recorded_size << " " << queuesize() << endl;
    _max_recorded_size = 0;
    _flows_at_queues.clear();
    _prev_txbytes = _txbytes;
    _prev_sample_t = crt_t;
}


// NIC QUEUE

HbHNICQueue::HbHNICQueue(linkspeed_bps bitrate, mem_b maxsize, 
                     EventList& eventlist, QueueLogger* logger,
                     int node, DynExpTopology *top,
                     unsigned D, unsigned P)
: HbHQueue(bitrate,maxsize,eventlist,logger,0,0,top,D,P)
{
    _next_tx_pkt = NULL;
    _servicing = false;
    _is_NIC = true;
    _is_timer_set = false;
    _is_downlink = false;
    _node = node;
    _D = D;
    _P = P;
}

void
HbHNICQueue::setupNICPkt(Packet* pkt) {
    pkt->set_fabricts(eventlist().now());
    pkt->set_src_ToR(_top->get_firstToR(pkt->get_src())); // set the sending ToR for SRC routing
    if (pkt->get_src_ToR() == _top->get_firstToR(pkt->get_dst())) {
        assert(0);
        // the packet is being sent within the same rack
        pkt->set_lasthop(false);
        pkt->set_crthop(-1);
        pkt->set_crtToR(-1);
        pkt->set_maxhops(0); // want to select a downlink port immediately
    } else {
        // the packet is being sent between racks
        // we will choose the path based on the current slice
        int slice = _top->time_to_slice(eventlist().now());
        // get the number of available paths for this packet during this slice
        int npaths = _top->get_no_paths(pkt->get_src_ToR(),
                _top->get_firstToR(pkt->get_dst()), slice);

        if (npaths == 0)
            cout << "Error: there were no paths for slice " << slice << " src "
                << pkt->get_src_ToR() << " dst "
                << _top->get_firstToR(pkt->get_dst()) << endl;
        assert(npaths > 0);

        // randomly choose a path for the packet
        // !!! todo: add other options like permutation, etc...
        int path_index = fast_rand() % npaths;
        // cout << "path_index " << path_index << endl;

        pkt->set_slice_sent(slice); // "timestamp" the packet
        pkt->set_fabricts(eventlist().now());
        pkt->set_path_index(path_index); // set which path the packet will take

        // set some initial packet parameters used for label switching
        pkt->set_lasthop(false);
        pkt->set_crthop(-1);
        pkt->set_crtToR(-1);
        pkt->set_maxhops(_top->get_no_hops(pkt->get_src_ToR(),
                    _top->get_firstToR(pkt->get_dst()), slice, path_index));
        // cout << "HOPS flow " << pkt->flow_id() << " " <<
        // _top->get_no_hops(pkt->get_src_ToR(), _top->get_firstToR(pkt->get_dst()),
        // slice, path_index) << endl;
    }
}

void 
HbHNICQueue::completeService() {
    /* dequeue the packet selected from extractNext() */
    assert(_next_tx_pkt != NULL);
    Packet* pkt = _next_tx_pkt;
    {
    HbHPacket *hbhp = (HbHPacket*)pkt;
    //cout << hbhp->flow_id() << " " << hbhp->seqno() << endl;
    }
    if(pkt->id() == 255024) {
        cout << "NIC COMPLETESERVICE slice " << _top->time_to_slice(eventlist().now()) << endl;
    }
    assert(_dst_to_queued[pkt->get_dst()/6] > 0);
    _dst_to_queued[pkt->get_dst()/6]--;
    assert(_flowid_to_queued[pkt->flow_id()] > 0);
    _flowid_to_queued[pkt->flow_id()]--;
    assert(_queuesize >= pkt->size());
    _queuesize -= pkt->size();

    assert(_next_tx_pkt != NULL);
    /* tell the packet to move on to the next pipe */
    assert(pkt->type() == HBHDATA);
    HbHPacket *hbhp = (HbHPacket*)(pkt);
    HbHSrc *hbhsrc = hbhp->get_hbhsrc();
    sendFromQueue(pkt);
    _servicing = false;

    //NIC needs the next packet pulled, if possible
    if (!_enqueued.empty()) {
        //check if any receive queue can take another packet
        list<Packet*>::reverse_iterator rit;
        int i = 0;
        for(rit = _enqueued.rbegin(); rit != _enqueued.rend(); ++rit) {
            //we are making all possible queues poll, but none of those may choose us...
            Packet *pkt = (*rit);
            //cout << "NIC completeService " << _node << " flowid " << pkt->flow_id() << " src " << pkt->get_src() << " dst " << pkt->get_dst() << " idx " << i << endl;
            Queue *q = getNextQueue(*(*rit));
            if(q == NULL) continue;
            q->pollme(this);
            //but if they do we called beginService(), so we stop checking
            if(_servicing) break; 
            i++;
        }
    }
    //pull next packet from hbhsrc
    hbhsrc->triggerSending();
}

void
HbHNICQueue::receivePacket(Packet & pkt)
{
    //cout << nodename() << " receivePacket " << pkt.flow_id() << " queuesize " << _queuesize << " " << eventlist().now() << endl;
    if(pkt.id() == 255024 || pkt.flow_id() == 632) {
        int slice = _top->time_to_slice(eventlist().now());
        cout << "NIC RECEIVEPACKET id " << pkt.id() << " flowid " << pkt.flow_id() << " slice " << slice << endl;
    }

    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();
    pkt.inc_queueing(_queuesize);
    pkt.set_last_queueing(_queuesize);
    _dst_to_queued[pkt.get_dst()/6]++;
    _flowid_to_queued[pkt.flow_id()]++;   
    //otherwise, uplink needs the next packet pulled, if possible
    //if not already servicing packet, check if next queue for packet can receive it
    if(!_servicing) {
        //cout << "NIC receivePacket\n";
        Queue *nextq = getNextQueue(pkt);
        assert(nextq); //this should always be present?
        //nextq->poll();
        nextq->pollme(this);
    }
}
