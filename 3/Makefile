all: test.o userfs.o
	gcc test.o userfs.o

test.o: test.c
	gcc -c -ggdb -O0 test.c -o test.o -I ../utils

userfs.o: userfs.c
	gcc -c -ggdb -O0 userfs.c -o userfs.o

clean:
	rm a.out *.o
