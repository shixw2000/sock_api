
RM=rm -f

VERSION:=$(shell date "+%F_%T")
COMILE_OPT=
MACROS=-D__BUILD_VER__="\"V1.0 $(VERSION)\""

ifdef rel
CC=g++ -s -O3 -W -Wall $(MACROS) $(COMILE_OPT)
libs=-ldl -lrt -pthread
else
CC=g++ -g -O0 -W -Wall  $(MACROS) $(COMILE_OPT) -D__TEST_POLL__
libs=-ldl -lrt -pthread
endif

prog:=bin_prog
libname:=libsockapi.so

inc_dir=-Iextern
lib_dir=-L../libs

lib_srcs=$(wildcard *.cpp) 
main_srcs=$(wildcard test/*.cpp)
	
lib_objs=$(lib_srcs:.cpp=.o)
main_objs=$(main_srcs:.cpp=.o)
objs=$(main_objs) $(lib_objs) 

default:$(prog)
.PHONY:default

lib:$(libname)
.PHONY:lib

ifdef __test__
$(prog):$(main_objs) $(libname)
	$(CC) -o $@ $^ $(lib_dir) $(libs)

else
$(prog):$(main_objs) $(lib_objs)
	$(CC) -o $@ $^ $(lib_dir) $(libs)

endif

$(libname):$(lib_objs) 
	$(CC) -shared -o $@ $^ 

$(lib_objs):%.o:%.cpp
	$(CC) -fPIC -c -o $@ $< $(inc_dir)	
	
$(main_objs):%.o:%.cpp
	$(CC) -c -o $@ $< $(inc_dir)

clean:
	-@$(RM)  $(objs) $(prog) $(libname)
.PHONY:clean

run:$(prog)
	export LD_LIBRARY_PATH=. && ./$(prog)
.PHONY:run


