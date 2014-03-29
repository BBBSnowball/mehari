# needed environment variables
# (shold be set by the reconos toolchain)
# RECONOS
# CROSS_COMPILE

TARGET_CC=$(CROSS_COMPILE)gcc

NAME=single_pendulum_simple

CFLAGS += -O0 -g -Wall -static -L $(RECONOS)/linux/lib -I $(RECONOS)/linux/lib/include

CFLAGS_HOST = $(CFLAGS) -DWITHOUT_RECONOS

APP_OBJS = timing.o data.o $(NAME).o

all: $(NAME) $(NAME)_host

$(NAME): $(APP_OBJS)
	$(TARGET_CC) $(APP_OBJS) $(CFLAGS) -o $(NAME) -static -lreconos -lpthread -lm -lrt

clean:
	rm -f *.o $(NAME) $(NAME)_host

%.o: %.c
	$(TARGET_CC) -c $(CFLAGS) -o $@ $<

test-without-reconos: $(NAME)
	qemu-arm-static $(NAME) 0 2 --without-reconos

$(NAME)_host: $(patsubst %.o,%.host.o,$(APP_OBJS))
	$(CC) $(patsubst %.o,%.host.o,$(APP_OBJS)) $(CFLAGS_HOST) -o $@ -static -lreconos_host -lpthread -lm -lrt

%.host.o: %.c
	$(CC) -c $(CFLAGS_HOST) -o $@ $<
