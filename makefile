
# ===================================== 
# Assignment 6 Submission 
# Name: NAGALLA DEVISRI PRASAD
# Roll number: 22CS10045 
# =====================================



# Makefile for My_SMTP Client and Server

CC = gcc
CFLAGS = -Wall -Wextra -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -D_DEFAULT_SOURCE
TARGETS = mysmtp_client mysmtp_server

all: $(TARGETS)

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o mysmtp_client mysmtp_client.c

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o mysmtp_server mysmtp_server.c

clean:
	rm -f $(TARGETS)

