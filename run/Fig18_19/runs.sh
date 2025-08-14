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

FNAME=flare55us_ws35perc_0.5s_60c30t_40a_a4b16_fbw12.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 60 -qshaping 30 -aeolus 40 -tent 4 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/websearch_35percLoad_500msec_648hosts.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

FNAME=clos_bolt_ws30perc_0.5s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/clos/datacenter/htsim_bolt_fatTree -simtime ${SIMTIME_FULL} -nodes 648 -utiltime 0.1 -cwnd 40 -q 600 -flowfile ../../traffic/websearch_clos_30percLoad_500msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"

FNAME=clos_bolt_ws35perc_0.5s_1q.txt
FILES+=($FNAME)
COMMAND="../../src/clos/datacenter/htsim_bolt_fatTree -simtime ${SIMTIME_FULL} -nodes 648 -utiltime 0.1 -cwnd 40 -q 1200 -flowfile ../../traffic/websearch_clos_35percLoad_500msec_648hosts.htsim > ../output/$FNAME"
run_sim
printf "\n"


echo "Finished running simulations, plotting figures..."
if [[ $SUCCESS = false ]]; then
    echo "Warning: some simulations seem to have failed!!! Figures may not plot successfully!"
fi

python3 ../plot/plot_fct99th.py clos $BASEDIR
python3 ../plot/plot_1util.py clos30 $BASEDIR
python3 ../plot/plot_1util.py clos35 $BASEDIR

echo "Done!"
