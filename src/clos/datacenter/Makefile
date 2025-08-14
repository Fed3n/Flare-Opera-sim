CC = g++
CFLAGS = -Wall -g -O
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

all:	htsim_hpcc_fatTree htsim_bolt_fatTree htsim_dctcp_fatTree htsim_dctcp_fatTree1to1 htsim_tcp_fatTree htsim_ndp_fatTree htsim_xpass_fatTree htsim_xpass_fatTree1to1

htsim_hpcc_fatTree: main_hpcc_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_hpcc_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_hpcc_fatTree

htsim_bolt_fatTree: main_bolt_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_bolt_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_bolt_fatTree

htsim_dctcp_fatTree: main_dctcp_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_dctcp_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_dctcp_fatTree

htsim_dctcp_fatTree1to1: main_dctcp_fatTree1to1.o firstfit.o ../libhtsim.a fat_tree_topology.o
	$(CC) $(CFLAGS) firstfit.o main_dctcp_fatTree1to1.o fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_fatTree1to1

htsim_tcp_fatTree: main_tcp_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_tcp_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_tcp_fatTree
# htsim_tcp_fatTree: main_tcp_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology.o
# 	$(CC) $(CFLAGS) firstfit.o main_tcp_fatTree.o fat_tree_topology.o $(LIB) -lhtsim -o htsim_tcp_fatTree

htsim_ndp_fatTree: main_ndp_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology.o
	$(CC) $(CFLAGS) firstfit.o main_ndp_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_ndp_fatTree

htsim_xpass_fatTree: main_xpass_fatTree.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_xpass_fatTree.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_xpass_fatTree

htsim_xpass_fatTree1to1: main_xpass_fatTree1to1.o firstfit.o ../libhtsim.a fat_tree_topology.o
	$(CC) $(CFLAGS) firstfit.o main_xpass_fatTree1to1.o fat_tree_topology.o $(LIB) -lhtsim -o htsim_xpass_fatTree1to1

htsim_ndp_fatTree_3to1_k12: main_ndp_fatTree_3to1_k12.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_ndp_fatTree_3to1_k12.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_ndp_fatTree_3to1_k12

# htsim_ndp_fatTree_3to1_k24: main_ndp_fatTree_3to1_k24.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k24.o
# 	$(CC) $(CFLAGS) firstfit.o main_ndp_fatTree_3to1_k24.o fat_tree_topology_3to1_k24.o $(LIB) -lhtsim -o htsim_ndp_fatTree_3to1_k24

htsim_ndp_fatTree_6to1_hybrid: main_ndp_fatTree_6to1_hybrid.o firstfit.o ../libhtsim.a fat_tree_topology_6to1_hybrid.o
	$(CC) $(CFLAGS) firstfit.o main_ndp_fatTree_6to1_hybrid.o fat_tree_topology_6to1_hybrid.o $(LIB) -lhtsim -o htsim_ndp_fatTree_6to1_hybrid

main_hpcc_fatTree.o: main_hpcc_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_hpcc_fatTree.cpp

main_bolt_fatTree.o: main_bolt_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_bolt_fatTree.cpp

main_dctcp_fatTree.o: main_dctcp_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_fatTree.cpp

main_dctcp_fatTree1to1.o: main_dctcp_fatTree1to1.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_fatTree1to1.cpp

main_tcp_fatTree.o: main_tcp_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_fatTree.cpp

main_ndp_fatTree.o: main_ndp_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_fatTree.cpp

main_xpass_fatTree.o: main_xpass_fatTree.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_xpass_fatTree.cpp

main_xpass_fatTree1to1.o: main_xpass_fatTree1to1.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_xpass_fatTree1to1.cpp

main_ndp_fatTree_3to1_k12.o: main_ndp_fatTree_3to1_k12.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_fatTree_3to1_k12.cpp 

# main_ndp_fatTree_3to1_k24.o: main_ndp_fatTree_3to1_k24.cpp
# 	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_fatTree_3to1_k24.cpp

main_ndp_fatTree_6to1_hybrid.o: main_ndp_fatTree_6to1_hybrid.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_fatTree_6to1_hybrid.cpp


firstfit.o: firstfit.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c firstfit.cpp

fat_tree_topology.o: fat_tree_topology.cpp fat_tree_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology.cpp
 	
fat_tree_topology_3to1_k12.o: fat_tree_topology_3to1_k12.cpp fat_tree_topology_3to1_k12.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology_3to1_k12.cpp

# fat_tree_topology_3to1_k24.o: fat_tree_topology_3to1_k24.cpp fat_tree_topology_3to1_k24.h topology.h
# 	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology_3to1_k24.cpp

fat_tree_topology_6to1_hybrid.o: fat_tree_topology_6to1_hybrid.cpp fat_tree_topology_6to1_hybrid.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology_6to1_hybrid.cpp

clean:	
	rm -f *.o htsim_ndp* htsim_tcp* htsim_dctcp*
