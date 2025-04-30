# Ben Burnham EC535 Spring 2025

CC = arm-linux-gnueabihf-gcc	# Compiler
CFLAGS = # -static	# Flags
SRC = inputtimer.c	# Source
OUT = inputtimer	# Output file

# Cross-compile kernel module using specs above with $make
default:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS)

# Clean up the generated files
clean:
	rm -f $(OUT)
