OUT=./bin/servidor

SOURCES  = ./src/device.c ./src/config_async.c ./src/main.c ./src/log.c ./src/filter.c

INCLUDE = ./inc
ARM= arm-linux-gnueabihf-gcc
CC = gcc

all: $(OUT)

./arm/servidor: $(SOURCES) 
	@mkdir -p arm
	$(ARM)  $(SOURCES) -o ./arm/servidor -I $(INCLUDE) -lrt -lpthread -g -Wall
	@echo "Cross Compilacion Correcta."

arm: ./arm/servidor

scp: ./arm/servidor
	scp arm/servidor ubuntu@192.168.0.14:/home/ubuntu/bin


$(OUT): $(SOURCES) 
	@mkdir -p bin
	$(CC)  $(SOURCES) -o $(OUT) -I $(INCLUDE) -lrt -lpthread -g -Wall
	@echo "Compilacion Correcta."

run: $(OUT)
	./bin/servidor

asm: ./src/filter.c
	$(ARM) ./src/filter.c -S -Ofast -o filter_fast.s -I $(INCLUDE)
	$(ARM) ./src/filter.c -S -o filter_normal.s -I $(INCLUDE)
	@echo "Compilacion de filter.c exitosa."

clean:
	rm -rf bin/ arm/ filter_fast.s filter_normal.s