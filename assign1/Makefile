# Compiler and flags
CC = gcc
CFLAGS = -g -Wall

# Source files
SRCS = dberror.c storage_mgr.c test_assign1_1.c

# Object files (corresponding .o files)
OBJS = $(SRCS:.c=.o)

# Output executable
EXEC = test_program

# Default target: build everything
all: $(EXEC)

# Link object files to create the final executable
$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC)

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJS) $(EXEC)
