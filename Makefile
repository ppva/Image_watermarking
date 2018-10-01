CC = /usr/local/bin/g++
CFLAGS += -std=c++11 -Wall -pedantic -fsanitize=address -g -Dcimg_display=0 -pthread
INCLUDES = -I./src
LDFLAGS += 

SRC = src
OUT = out

OBJECTS = $(OUT)/my_utils.o

FOBJECTS = $(OUT)/ffwatermarker.o 

NOBJECTS = $(OUT)/watermarker.o 

SOBJECTS = $(OUT)/seqwatermarker.o

MOBJECTS = $(OUT)/middleffwatermarker.o

$(OUT)/%.o: $(SRC)/%.cpp
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

default: $(OBJECTS)	$(NOBJECTS)	$(FOBJECTS) $(SOBJECTS) $(MOBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/watermarker $(OBJECTS) $(NOBJECTS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/ffwatermarker $(OBJECTS) $(FOBJECTS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/seqwatermarker $(OBJECTS) $(SOBJECTS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/middleffwatermarker $(OBJECTS) $(MOBJECTS) $(LDFLAGS)

noff: $(OBJECTS) $(NOBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/watermarker $(OBJECTS) $(NOBJECTS) $(LDFLAGS)

seq: $(OBJECTS) $(SOBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/seqwatermarker $(OBJECTS) $(SOBJECTS) $(LDFLAGS)

ff: $(OBJECTS) $(FOBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/ffwatermarker $(OBJECTS) $(FOBJECTS) $(LDFLAGS)

middle: $(OBJECTS) $(MOBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT)/middleffwatermarker $(OBJECTS) $(MOBJECTS) $(LDFLAGS)
clean:
	rm -rf $(OUT)

