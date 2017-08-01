TARGET = ppb_search_strings
OBJS = ppb_search_strings.o

CC          = gcc
CFLAGS      = -O2
LDFLAGS     =
INCLUDES    = -I./
LIBS        = -lm -lpthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): timer.h

.c.o:
		$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
			rm -rf $(TARGET) $(OBJS) *.exe *~
