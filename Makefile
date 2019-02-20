CXX = g++
CXXFLAGS = -std=c++14 -Wall `pkg-config --cflags libndn-cxx` -g
LIBS = `pkg-config --libs libndn-cxx`
DESTDIR ?= /usr/local
SOURCE_OBJS = server-daemon.o nd-client.o nd-server.o
PROGRAMS = nd-client nd-server

all: $(PROGRAMS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(LIBS)

nd-server: $(SOURCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ server-daemon.o nd-server.o $(LIBS)

nd-client: $(SOURCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ nd-client.o $(LIBS)

clean:
	rm -f $(PROGRAMS)

install: all
	cp $(PROGRAMS) $(DESTDIR)/bin/

uninstall:
	cd $(DESTDIR)/bin && rm -f $(PROGRAMS)