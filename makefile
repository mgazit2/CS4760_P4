C		= gcc
CFLAGS		= -Wall -g -lm

SOSS		= oss.c
OOSS		= $(SOSS:.c=.o) $(OUTIL) $(OQUEUE)
OSS		= oss

SUSER	= child.c
OUSER	= $(SUSER:.c=.o) $(OUTIL)
USER		= child

OUTIL	= util.o

OQUEUE	= queue.o

OUTPUT		= $(OSS) $(USER)

all: $(OUTPUT)

$(OSS): $(OOSS)
	$(CC) $(CFLAGS) $(OOSS) -o $(OSS)

$(USER): $(OUSER)
	$(CC) $(CFLAGS) $(OUSER) -o $(USER)

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.PHONY: clean
clean:
	/bin/rm -f $(OUTPUT) *.o logfile
