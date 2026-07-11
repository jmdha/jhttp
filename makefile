test:
	gcc -Wall -ggdb test.c -o test.out -lcriterion
	./test.out

fuzz_request:
	clang -g -O1 -fsanitize=fuzzer,address,undefined fuzz_request.c -o fuzz.out
	./fuzz.out -max_len=1048576 corpus/request
