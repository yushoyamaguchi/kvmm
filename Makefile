COMPILER = gcc
TARGET = kvmm
CFLAGS = -pthread
OBJECTS = main.o util.o blk.o

all: 
	make $(TARGET)

%.o: %.c Makefile
	$(COMPILER) $(CFLAGS) -c $<

$(TARGET): $(OBJECTS) Makefile
	gcc -o $(TARGET) $(CFLAGS) $(OBJECTS)

clean:
	rm -rf $(OBJECTS) kvmm out.txt; touch out.txt;