#!/bin/bash

rm -f out.txt;
for i in {1..10}
do 
	filename=$i".txt";
	rm -f filename;
	python3 generator.py -f $filename -c 1000000 -m 1000000;
done;

gcc main.c libcoro.c;
./a.out 1000 3 1.txt 2.txt 3.txt 4.txt 5.txt 6.txt 7.txt 8.txt 9.txt 10.txt;

python3 checker.py -f out.txt;
rm -f out.txt;
rm -f a.out;

for i in {1..10}
do 
	filename=$i".txt";
	rm -f $filename;
done 

