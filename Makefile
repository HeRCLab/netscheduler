COMPILER	= gcc
C_OPTS		= -g -Wno-format-truncation
SOURCES		= $(filter-out network.c,$(wildcard *.c))

schednet: $(SOURCES) netscheduler.h
	-rm -f network.c
	$(COMPILER)  $(C_OPTS) $(SOURCES) -o $@
