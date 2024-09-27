FUZIX_ROOT = /opt/fuzix

include $(FUZIX_ROOT)/Target/rules.z80

SRCS  = httpd5.c

OBJS = $(SRCS:.c=.o)

APPS = httpd

all: $(APPS)

httpd: $(OBJS)
	$(LINKER) $(LINKER_OPT) -o httpd5 $(CRT0) httpd5.o $(LINKER_TAIL)

size.report: $(APPS)
	ls -l $^ > $@

clean:
	rm -f $(OBJS) $(APPS) $(SRCS:.c=) core *~ *.asm *.lst *.sym *.map *.noi *.lk *.ihx *.tmp *.bin size.report *~

rmbak:
	rm -f *~ core