#!/bin/bash

#get relative path
PARENT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$PARENT_PATH"
source ../run_util.sh

echo "Running simulations for ${SIMTIME_FULL}s simtime"
printf "\n"

FNAME=flare55us_ws30perc_0.5s_60c30t_40a_a4b16_fbw12.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 60 -qshaping 30 -aeolus 40 -tent 4 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_500msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=flare15us_ws30perc_0.3s_16c8t_8a_a4b16_fbw12.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 16 -qshaping 8 -aeolus 8 -tent 2 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=bolt55us_ws30perc_0.5s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_bolt_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -utiltime 0.1 -cwnd 40 -q 600 -topfile ../../topologies/dynexp_55us_symm.txt -flowfile ../../traffic/websearch_30percLoad_500msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=bolt15us_ws30perc_0.3s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_bolt_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -utiltime 0.1 -cwnd 40 -q 600 -topfile ../../topologies/dynexp_15us_symm.txt -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=xpass55us_ws30perc_0.3s_80c_40a_05winit.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 80 -aeolus 40 -winit 0.5 -tloss 0.1 -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=xpass15us_ws30perc_0.3s_20c_8a_05winit.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 20 -aeolus 8 -winit 0.5 -tloss 0.1 -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=hbh55us_ws30perc_0.3s_10P_5D.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_hbh_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -hbhd 5 -hbhp 10 -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=hbh15us_ws30perc_0.3s_6P_2D.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_hbh_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -hbhd 2 -hbhp 6 -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_15us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=tdtcp55us_ws30perc_0.3s_40ecn_nosyn_10cwnd.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_dctcp_dynexpTopology -simtime ${SIMTIME_FULL} -cutoff 1500000000 -tdtcp -utiltime 0.1 -cwnd 10 -q 600 -topfile ../../topologies/dynexp_55us_symm.txt -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=ndp55us_ws30perc_0.3s_80q.txt
FILES+=($FNAME)
#ndp is more resource intensive then other protocols to simulate, we only have it for <300ms
SIMTIME_NDP=0.25
if (( $(bc <<< "$SIMTIME > $SIMTIME_NDP" ) )); then
    SIMTIME=$SIMTIME_NDP
    SIMTIME_FULL=${SIMTIME_NDP}01
    SIMTIME_MS=$(echo $SIMTIME_FULL*1000 | bc)
fi
echo $SIMTIME_FULL
echo $SIMTIME_MS
COMMAND="../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime ${SIMTIME_FULL} -cwnd 40 -q 80 -pullrate 1.0 -strat single -cutoff 15000000000 -utiltime 0.1 -flowfile ../../traffic/websearch_30percLoad_300msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

echo "Finished running simulations, plotting figures..."
if [[ $SUCCESS = false ]]; then
    echo "Warning: some simulations seem to have failed!!! Figures may not plot successfully!"
fi

python3 ../plot/plot_fct99th.py ws30perc $BASEDIR
python3 ../plot/plot_2util.py $BASEDIR
python3 ../plot/plot_2qcdf.py $BASEDIR

echo "Done!"
