all: pvrsrvinit

pvrsrvinit: pvrsrvinit.c
	gcc -Wall -Wextra -std=c99 -ldl $< -o $@

clean:
	rm -f pvrsrvinit
