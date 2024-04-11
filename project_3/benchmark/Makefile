
all : test
test: ../my_vm.h
	gcc test.c -L../ -lm -lmy_vm -m32 -o test
	gcc multi_test.c -L../ -lm -lmy_vm -m32 -o mtest -lpthread
	gcc our_test.c -L../ -lm -lmy_vm -m32 -o otest -lpthread

clean:
	rm -rf test mtest otest


