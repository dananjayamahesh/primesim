CC=g++
CFLAGS=-lpthread -std=c++11 
PIN_VERSION_SCRIPT = $(shell find $(PINPATH) -name pintool.ver)
OBJECTS=program.o

PIN_CXX_FLAGS := -Wall -Werror -Wno-unknown-pragmas  -O3 -fomit-frame-pointer \
           -DBIGARRAY_MULTIPLIER=1 -DUSING_XED  -fno-strict-aliasing \
           -D_GLIBCXX_USE_CXX11_ABI=0 -fabi-version=2 \
           -I$(LIBXML2_PATH) -g3 -I$(PINPATH)/source/tools/Include \
           -I$(PINPATH)/source/tools/InstLib -I$(PINPATH)/extras/xed-intel64/include \
           -I$(PINPATH)/extras/components/include -I$(PINPATH)/source/include \
           -I$(PINPATH)/source/include/pin \
           -I$(PINPATH)/source/include/pin/gen \
           -I$(PINPATH)/source/include/gen -fno-stack-protector -DTARGET_IA32E -DHOST_IA32E \
           -fPIC -DTARGET_LINUX 

PIN_LD_FLAGS :=  -Wl,--hash-style=sysv -shared -Wl,-Bsymbolic \
           -Wl,--version-script=$(PIN_VERSION_SCRIPT)  \
           -L$(PINPATH)/source/tools/Lib/ -lxml2 -lz -lm -g3 -O3 \
           -L$(PINPATH)/source/tools/ExtLib/ -L$(PINPATH)/extras/xed-intel64/lib -L$(PINPATH)/intel64/lib \
           -L$(PINPATH)/intel64/lib-ext  -lpin  -lxed -lpindwarf -ldl -lrt

.PHONY: all clean

all: client

clean:
	rm -f program.o program

client: $(OBJECTS)
	$(CC) $(PIN_CXX_FLAGS) $(OBJECTS) -o client $(PIN_LD_FLAGS)

obj/%.o: src/%.cpp dep/%.d 
	mpic++ -c $< -o $@ $(CXX_FLAGS)