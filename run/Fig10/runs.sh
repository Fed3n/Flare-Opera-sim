#!/bin/bash

#get relative path
PARENT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$PARENT_PATH"
source ../run_util.sh

SIMTIME=7.0
SIMTIME_FULL=${SIMTIME}01 #always add slighly more
SIMTIME_MS=$(echo $SIMTIME*1000 | bc) #to check for simtime inside output file

echo "Running simulations for ${SIMTIME_FULL}s simtime"
printf "\n"

FNAME=flare_fairshare.txt
FILES+=($FNAME)
COMMAND="../../src/opera/datacenter/htsim_xpass_dynexpTopology -flare -simtime ${SIMTIME_FULL} -cwnd 40 -q 600 -credq 60 -qshaping 30 -aeolus 40 -tent 4 -winit 1.0 -tloss 0.1 -fbw 1.2 -jita 4 -jitb 16 -fbsens -tp 5.94 -probfile ../pfun_exp2.txt -utiltime 0.1 -flowfile ../../traffic/fairshare_flows.htsim -topfile ../../topologies/dynexp_55us_symm.txt > ../output/$FNAME"
run_sim
printf "\n"

echo "Finished running simulations, plotting figures..."
if [[ $SUCCESS = false ]]; then
    echo "Warning: some simulations seem to have failed!!! Figures may not plot successfully!"
fi

python3 ../plot/plot_fairshare.py $BASEDIR

echo "Done!"
