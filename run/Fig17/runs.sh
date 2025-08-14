#!/bin/bash

#get relative path
PARENT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$PARENT_PATH"
source ../run_util.sh

echo "Running simulations for ${SIMTIME_FULL}s simtime"
printf "\n"

FNAME=flare55us_rpc30perc_0.5s_60c30t_40a_a4b16_fbw12.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 60 -qshaping 30 -aeolus 40 -tent 4 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_500msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=flare15us_rpc30perc_0.3s_16c8t_8a_a4b16_fbw12.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 16 -qshaping 8 -aeolus 8 -tent 2 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=bolt55us_rpc30perc_0.5s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_bolt_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -utiltime 0.1 -cwnd 40 -q 600 -topfile ../../topologies/dynexp_55us_symm.txt -flowfile ../../traffic/boltrpc_30percLoad_500msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=bolt15us_rpc30perc_0.3s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_bolt_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -utiltime 0.1 -cwnd 40 -q 600 -topfile ../../topologies/dynexp_15us_symm.txt -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=xpass55us_rpc30perc_0.3s_80c_40a_05winit.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 80 -aeolus 40 -tent 4 -winit 0.5 -tloss 0.1 -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=xpass15us_rpc30perc_0.3s_20c_8a_05winit.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 20 -aeolus 8 -tent 4 -winit 0.5 -tloss 0.1 -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=hbh55us_rpc30perc_0.3s_10P_5D.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_hbh_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -hbhd 5 -hbhp 10 -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=hbh15us_rpc30perc_0.3s_6P_2D.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_hbh_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -hbhd 2 -hbhp 6 -utiltime 0.1 -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=tdtcp55us_rpc30perc_0.3s_40ecn_nosyn_10cwnd.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_dctcp_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -tdtcp -utiltime 0.1 -cwnd 10 -q 600 -topfile ../../topologies/dynexp_55us_symm.txt -flowfile ../../traffic/boltrpc_30percLoad_300msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

echo "Finished running simulations, plotting figures..."
if [[ $SUCCESS = false ]]; then
    echo "Warning: some simulations seem to have failed!!! Figures may not plot successfully!"
fi

python3 ../plot/plot_fct99th.py rpc30perc $BASEDIR

echo "Done!"
