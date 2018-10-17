all: prog
prog: minish.c
	gcc minish.c -o minish
	gcc test.c -o test
clean:
	rm minish
	rm test
run:
	./minish
