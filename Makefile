CC = gcc
CFLAGS = -Wall -g -std=gnu89 -pedantic
TARGET = httpd tftpd test
SRCS = httpd.c tftpd.c test.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

httpd: $(OBJS)
	$(CC) $(CFLAGS) -o httpd httpd.o

tftpd: $(OBJS)
	$(CC) $(CFLAGS) -o tftpd tftpd.o

test: test.o
	$(CC) $(CFLAGS) -o test test.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean