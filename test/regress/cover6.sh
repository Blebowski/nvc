set -xe

pwd
which nvc

# Track only from U
nvc -a $TESTDIR/regress/cover6.vhd -e --cover=toggle,count-from-undefined cover6 -r
nvc --cover-report -o html cover6.ncdb 2>&1 | grep -v '^** Debug:' | tee out.txt

# Track only from/to Z
nvc -a $TESTDIR/regress/cover6.vhd -e --cover=toggle,count-from-to-z cover6 -r
nvc --cover-report -o html cover6.ncdb 2>&1 | grep -v '^** Debug:' | tee -a out.txt

# Track both
nvc -a $TESTDIR/regress/cover6.vhd -e --cover=toggle,count-from-undefined,count-from-to-z cover6 -r
nvc --cover-report -o html cover6.ncdb 2>&1 | grep -v '^** Debug:' | tee -a out.txt

if [ ! -f html/index.html ]; then
  echo "missing coverage report"
  exit 1
fi

diff -u $TESTDIR/regress/gold/cover6.txt out.txt
