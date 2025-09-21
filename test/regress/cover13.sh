set -xe

pwd
which nvc

nvc -a $TESTDIR/regress/cover13.vhd -e --cover=all --cover-spec=$TESTDIR/regress/data/cover13_spec.txt cover13 -r
nvc --cover-report -V -o html cover13.ncdb 2>&1 | grep -v '^** Debug:' | tee out.txt

# Filter out info lines with verbose that throw amount of allocated memory
sed -i -e 1,3d out.txt

diff -u $TESTDIR/regress/gold/cover13.txt out.txt
