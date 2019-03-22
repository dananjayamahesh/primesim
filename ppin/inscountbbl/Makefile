program_name = program1

PIN_VERSION_SCRIPT = $(shell find $(PINPATH) -name pintool.ver)

TOP_LEVEL_PROGRAM_NAME := bin/$(program_name).so
CXX_FILES := $(wildcard src/*.cc)
DEP_FILES := $(CXX_FILES:src/%.cc=dep/%.d)
O_FILES := $(filter-out obj/$(program_name).o, $(CXX_FILES:src/%.cc=obj/%.o)))
PIN_O_FILES := obj/$(program_name).o


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

CXX_FLAGS := -Wall -Werror -Wno-unknown-pragmas -O3 -g3 -lrt -I$(LIBXML2_PATH)

LD_FLAGS := -lxml2 -lz -lm -g3 -O3 -ldl -lrt 
           

.PHONY: clean remove 

all: $(TOP_LEVEL_PROGRAM_NAME)

obj/$(program_name).o: src/$(program_name).cc dep/$(program_name).d
	g++ -c $< -o $@ $(PIN_CXX_FLAGS)

obj/%.o: src/%.cc dep/%.d 
	g++ -c $< -o $@ $(CXX_FLAGS)

dep/$(program_name).d: src/$(program_name).cc
	g++ -MM $(PIN_CXX_FLAGS) -MT '$(patsubst src/%.cc,obj/%.o,$<)' $< -MF $@

dep/%.d: src/%.cc
	g++ -MM $(CXX_FLAGS) -MT '$(patsubst src/%.cc,obj/%.o,$<)' $< -MF $@

bin/$(program_name): $(O_FILES)
	g++ $^ -o $@ $(LD_FLAGS)

bin/$(program_name).so: $(PIN_O_FILES)
	g++ $^ -o $@ $(PIN_LD_FLAGS)

clean:
	rm -f dep/*.d obj/*.o bin/* 

remove:
	rm -f dep/*.d obj/*.o bin/*