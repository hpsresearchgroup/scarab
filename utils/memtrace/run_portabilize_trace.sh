SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

DRIO_BUILD_DIR=${SCRIPTDIR}/../../src/build/opt/deps/dynamorio

for dir in */; do
    echo "$dir"
    cd $dir
    mkdir -p bin
    cp raw/modules.log bin/modules.log
    python2 $SCRIPTDIR/portabilize_trace.py .
    cp bin/modules.log raw/modules.log
    ${DRIO_BUILD_DIR}/clients/bin64/drraw2trace -indir ./raw/ &
    cd -
done
