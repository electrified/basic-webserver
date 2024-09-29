CC = gcc
CFLAGS = -Wall -g -std=gnu89 -pedantic
TARGET = httpd tftpd
SRCS = httpd.c tftpd.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

httpd: $(OBJS)
	$(CC) $(CFLAGS) -o httpd httpd.o

tftpd: $(OBJS)
	$(CC) $(CFLAGS) -o tftpd tftpd.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean