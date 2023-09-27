all:
	gcc -g server.c -D_REENTRANT -o server -lpthread
clean:
	rm server
