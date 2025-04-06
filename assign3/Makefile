# Compiler and flags
CC = gcc
CFLAGS = -g -Wall

# Source files
SRCS = record_mgr.c expr.c rm_serializer.c buffer_mgr.c buffer_mgr_stat.c dberror.c storage_mgr.c test_assign3_1.c test_expr.c

# Object files (corresponding .o files)
OBJS = $(SRCS:.c=.o)

# Output executable
EXEC = test_program_2

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

