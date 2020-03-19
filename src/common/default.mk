OBJS = main.o test.o
PROG = run_test
ASFLAGS += -g
CFLAGS += -g

vpath main.c ../common

ifeq ($(origin NFUNCS), undefined)
	NFUNCS=2
endif

ifeq ($(origin NCALLS), undefined)
	NCALLS=100000
endif

ifeq ($(origin PERCENTAGE), undefined)
	PERCENTAGE=0
endif

ifeq ($(origin CORES), undefined)
	CORES=0
endif

ifeq ($(origin PERFEVENTS), undefined)
	PERFEVENTS="instructions,cycles"
endif

ifeq ($(origin CS), undefined)
	CS=$(shell basename "${PWD}")
endif

RESULTFILE = $(CS)_results_$(NFUNCS)_$(NCALLS)_$(PERCENTAGE)_$(CORES).out
PERFFILE = $(CS)_perf_$(NFUNCS)_$(NCALLS)_$(PERCENTAGE)_$(CORES).out

all : test.o $(PROG)

.PHONY : all run run_test test.s clean run run100

test.s : make_test_s
	./make_test_s $(NFUNCS) $(NCALLS) $(PERCENTAGE) > test.s

run_test : $(OBJS)
	$(CC) -o $(PROG) $(OBJS)

clean :
	rm -f $(PROG) test.s $(OBJS) *.out

run : $(PROG)
	taskset -c $(CORES) ./run_test > $(RESULTFILE)

run100 : $(PROG)
	-rm -f $(RESULTFILE)
	for i in $$(seq 1 100); do\
		taskset -c $(CORES) ./run_test >> $(RESULTFILE);\
	done

runperf100 : $(PROG)
	-rm -f $(RESULTFILE)
	-rm -f $(PERFFILE)
	for i in $$(seq 1 100); do\
		taskset -c $(CORES) perf stat -e $(PERFEVENTS) -x , -a -C $(CORES) -- ./run_test >> $(RESULTFILE) 2>>$(PERFFILE); \
	done
