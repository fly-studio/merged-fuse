CXX = g++
LD = g++

INC = -Iinclude
CFLAGS = -Wall -std=c++11 -g -O2
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = `pkg-config fuse --cflags --libs` -s
OBJDIR = obj
OUT = bin/merged-fuse
DEP = 
OBJS = $(OBJDIR)/merged-fuse.o $(OBJDIR)/base64.o $(OBJDIR)/concat.o

edit: $(OBJS)
	$(LD) $(LIBDIR) -o $(OUT) $(OBJS)  $(LDFLAGS) $(LIB)

$(OBJDIR)/merged-fuse.o: merged-fuse.cpp include/common.h
	$(CXX) $(CFLAGS) $(INC) -c merged-fuse.cpp -o $(OBJDIR)/merged-fuse.o

$(OBJDIR)/base64.o: src/base64.cpp
	$(CXX) $(CFLAGS) $(INC) -c src/base64.cpp -o $(OBJDIR)/base64.o

$(OBJDIR)/concat.o: src/concat.cpp include/common.h
	$(CXX) $(CFLAGS) $(INC) -c src/concat.cpp -o $(OBJDIR)/concat.o




clean:
	rm $(OBJS) $(OUT)
