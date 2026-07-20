bench:
	gcc -O3 -march=native bench.c -o bench.out
	./bench.out

test:
	gcc -Wall -ggdb -fsanitize=address test.c -o test.out
	./test.out

fuzz_request:
	clang -g -O1 -fsanitize=fuzzer,address,undefined fuzz_request.c -o fuzz.out
	./fuzz.out -max_len=1048576 -dict=http.dict
