CC = gcc
CFLAGS = -Wall -g -std=gnu89 -pedantic
TARGET = httpd2 tftpd
SRCS = httpd2.c tftpd.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

httpd2: httpd2.o
	$(CC) $(CFLAGS) -o httpd2 httpd2.o

tftpd: tftpd.o
	$(CC) $(CFLAGS) -o tftpd tftpd.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean