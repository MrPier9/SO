all: master users nodes

master: master.o my_lab.o
	gcc -std=c89 -pedantic -pthread master.o my_lab.o -o master

nodes: nodes.o my_lab.o
	gcc -std=c89 -pedantic -pthread nodes.o my_lab.o -o nodes

users: users.o my_lab.o
	gcc -std=c89 -pedantic -pthread users.o my_lab.o -o users

master.o: master.c
	gcc -c -Wall -std=c89 -pedantic -pthread master.c

nodes.o: nodes.c
	gcc -c -l -Wall -D_POSIX_C_SOURCE=199309L -std=c89 -pedantic nodes.c

users.o: users.c
	gcc -c -Wall -D_POSIX_C_SOURCE=199309L -std=c89 -pedantic users.c

my_lab.o: my_lab.c my_lab.h
	gcc -c -Wall -std=c89 -pedantic  my_lab.c

clean:
	rm -rf *.o master users nodes


