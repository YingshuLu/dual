# Compiler
CC = gcc

HDRS = $(wildcard *.h)

LIBS = -L./lib -lcask -lmaxminddb -lpthread -lcurl -lsqlite3 -ldl

# Compiler flags
CFLAGS = -I. -I./includes

# Source files
SRCS = $(wildcard *.c)

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = proxy

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) ${CFLAGS} -o $@ $^ ${LIBS} -v

# Compile source files to object files
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean