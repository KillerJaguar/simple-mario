CXXFLAGS=-std=c89 -ansi -Wall
CPPFLAGS=-I../tmx-parser
LDFLAGS=-lSDL -lSDL_mixer -lSDL_ttf
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,obj/%.o,$(SOURCES))
EXECUTABLE=mario
EXECDIR=./

all: $(SOURCES) $(EXECUTABLE)

nodeath: OPTIONS := -D DISABLE_DEATH
nodeath: all

debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3
release: all

clean:
	$(RM) $(OBJECTS) $(EXECDIR)$(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $(EXECDIR)$(EXECUTABLE) $(LDFLAGS)

$(OBJECTS): obj/%.o: %.c
	$(CC) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
