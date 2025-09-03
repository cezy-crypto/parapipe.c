CC=gcc
CFLAGS=-Wall -pthread

parapipe: parapipe.c
	$(CC) $(CFLAGS) -o parapipe parapipe.c
