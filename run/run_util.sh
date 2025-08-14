SIMTIME=0.5
OVERRIDE=false
TRY_FIND=true
SUCCESS=true
BASEDIR=output/
FILES=()

display_help() {
    echo "Syntax: $0 [options]"
    echo "-h            Display this help message."
    echo "-s simtime    Run simulations for simtime seconds (default: 0.6)."
    echo "-d directory  Use specified subdirectory (default: output/)."
    echo "-o            Force rerun and override of existing complete output files."
    echo "-f            Do not look for existing complete output files from other directories."
}

check_res() {
    if ! grep Util ../$BASEDIR$FNAME | grep -Fq $SIMTIME_MS; then
        echo "Failed to run $FNAME, file is incomplete!"
        SUCCESS=false
    else
        echo "Successfully finished running $FNAME!"
    fi
}

check_existing() {
    if [[ $OVERRIDE = true ]] || [[ $TRY_FIND = false ]]; then
        return 1
    fi
    #try finding same-name file to avoid rerunning experiment
    LOCS=$(find ../"$BASEDIR" -name "$FNAME")
    for LOC in $LOCS; do
        if grep Util $LOC | grep -Fq $SIMTIME_MS; then
            echo "Found existing and complete $LOC file in $BASEDIR, skipping..."
            return 0
        fi
    done
    return 1
}

run_sim () {
    echo "Checking for ${FNAME}..."
    if [[ $OVERRIDE = true ]] || ! [[ -e $FNAME ]] || ! grep Util $FNAME | grep -Fq $SIMTIME_MS; then
        check_existing
        if [[ $? -eq 1 ]]; then
            echo "Running ${FNAME}..."
            eval $COMMAND
            check_res
        fi
    else
        echo "Found complete ${FNAME}, skipping."
    fi
}

#get options and set all arguments
while getopts "hofs:d:" option; do
    case $option in
        h)  display_help
            exit;;
        o)  OVERRIDE=true;;
        f)  TRY_FIND=false;;
        s)  SIMTIME=$OPTARG;;
        d)  BASEDIR=$OPTARG;;
        \?)  echo "Invalid option: ${OPTARG}."
            display_help
            exit;;
    esac
done

SIMTIME_FULL=${SIMTIME}01 #always add slighly more
SIMTIME_MS=$(echo $SIMTIME*1000 | bc) #to check for simtime inside output file
