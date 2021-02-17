#!/bin/csh

/bin/rm outfile.txt
touch outfile.txt

foreach i (00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36)
  make test$i
  echo starting test $i ....  >> outfile.txt
  echo >> outfile.txt
  ./test$i >>& outfile.txt
  echo >> outfile.txt
  rm test$i.o test$i
end
