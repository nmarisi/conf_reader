HEADERS = conf_reader.h

default: conf_reader 

conf_reader.o: conf_reader.c $(HEADERS)
	gcc -c conf_reader.c -o conf_reader.o

conf_reader: conf_reader.o
	gcc conf_reader.o -o conf_reader

clean:
	-rm -f conf_reader.o
	-rm -f conf_reader
