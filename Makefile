all:
	gcc -Wall -o cache_sim cache_sim.c

run: all
	./cache_sim

clean:
	rm -f cache_sim
