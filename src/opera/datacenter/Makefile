CC = g++
CFLAGS = -Wall -ggdb -O
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
LIB=-L..


all:	htsim_ndp_dynexpTopology htsim_tcp_dynexpTopology htsim_dctcp_dynexpTopology htsim_dctcp_graphTopology htsim_hpcc_dynexpTopology htsim_bolt_dynexpTopology htsim_xpass_dynexpTopology htsim_xpass_graphTopology htsim_hbh_dynexpTopology



htsim_ndp_dynexpTopology: main_ndp_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_ndp_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_ndp_dynexpTopology

htsim_hpcc_dynexpTopology: main_hpcc_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_hpcc_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_hpcc_dynexpTopology

htsim_bolt_dynexpTopology: main_bolt_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_bolt_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_bolt_dynexpTopology

htsim_hbh_dynexpTopology: main_hbh_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_hbh_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_hbh_dynexpTopology

htsim_xpass_dynexpTopology: main_xpass_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_xpass_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_xpass_dynexpTopology

htsim_xpass_graphTopology: main_xpass_graphTopology.o ../libhtsim.a graph_topology.o dynexp_topology.o
	$(CC) $(CFLAGS) main_xpass_graphTopology.o graph_topology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_xpass_graphTopology

htsim_dctcp_dynexpTopology: main_dctcp_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_dctcp_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_dctcp_dynexpTopology

htsim_dctcp_graphTopology: main_dctcp_graphTopology.o ../libhtsim.a graph_topology.o dynexp_topology.o
	$(CC) $(CFLAGS) main_dctcp_graphTopology.o graph_topology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_dctcp_graphTopology

htsim_tcp_dynexpTopology: main_tcp_dynexpTopology.o ../libhtsim.a dynexp_topology.o
	$(CC) $(CFLAGS) main_tcp_dynexpTopology.o dynexp_topology.o $(LIB) -lhtsim -o htsim_tcp_dynexpTopology

main_ndp_dynexpTopology.o: main_ndp_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_dynexpTopology.cpp 

main_dctcp_dynexpTopology.o: main_dctcp_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_dynexpTopology.cpp 

main_dctcp_graphTopology.o: main_dctcp_graphTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_graphTopology.cpp 

main_hpcc_dynexpTopology.o: main_hpcc_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_hpcc_dynexpTopology.cpp 

main_bolt_dynexpTopology.o: main_bolt_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_bolt_dynexpTopology.cpp 

main_hbh_dynexpTopology.o: main_hbh_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_hbh_dynexpTopology.cpp 

main_xpass_dynexpTopology.o: main_xpass_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_xpass_dynexpTopology.cpp 

main_xpass_graphTopology.o: main_xpass_graphTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_xpass_graphTopology.cpp 

main_tcp_dynexpTopology.o: main_tcp_dynexpTopology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_dynexpTopology.cpp 

dynexp_topology.o: dynexp_topology.cpp dynexp_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c dynexp_topology.cpp

graph_topology.o: graph_topology.cpp graph_topology.h dynexp_topology.cpp dynexp_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c graph_topology.cpp

clean:	
	rm -f *.o htsim_*
