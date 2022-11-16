main: main.o mpuSim.o
	gcc -fdiagnostics-color=always -Wall -Werror=missing-prototypes -o main.out main.o mpuSim.o 
main.o: main.c mpuSim.h
	gcc -c -fdiagnostics-color=always -Wall -Werror=missing-prototypes -g main.c -lm
mpuSim.o: mpuSim.c mpuSim.h
	gcc -c -fdiagnostics-color=always -Wall -Werror=missing-prototypes -g mpuSim.c -lm