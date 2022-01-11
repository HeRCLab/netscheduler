COMPILER	= g++
C_OPTS		= -lm -g -ldl -Wno-format-truncation -I include
SOURCES		= $(filter-out network.c,$(wildcard *.c))

schednet: $(SOURCES) netscheduler.h
	-rm -f network.cpp
	$(COMPILER) $(SOURCES) $(C_OPTS) -o $@

clean:
	-rm schednet
