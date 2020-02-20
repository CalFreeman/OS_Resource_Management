CC=gcc
all: oss user

oss: oss.c
        $(CC) -o $@ $^ -pthread
user: user.c
        $(CC) -o $@ $^ -pthread

clean:
        rm oss user