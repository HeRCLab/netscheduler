COMPILER	= gcc
C_OPTS		= -lm -g -Wno-format-truncation
SOURCES		= $(filter-out network.c,$(wildcard *.c))

schednet: $(SOURCES) netscheduler.h
	-rm -f network.cpp
	$(COMPILER) $(SOURCES) $(C_OPTS) -o $@
