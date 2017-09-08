all: clean doit

doit: doit.o 
	g++ doit.o -o doit
	
doit.o: doit.cpp
	g++ -c doit.cpp
	
clean: 
	rm *.o doit -f