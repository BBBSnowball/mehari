# needed environment variables
# (shold be set by the reconos toolchain)
# CROSS_COMPILE
CC = $(CROSS_COMPILE)gcc

NAME=single_pendulum_simple

CFLAGS += -O0 -g -Wall -static -L $(RECONOS)/linux/lib -I $(RECONOS)/linux/lib/include

APP_OBJS = timing.o data.o $(NAME).o

all: $(NAME)

$(NAME): $(APP_OBJS)
	$(CC) $(APP_OBJS) $(CFLAGS) -o $(NAME) -static -lreconos -lpthread -lm -lrt

clean:
	rm -f *.o $(NAME) 

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
