CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGET  = ds_miniproject
SRC     = DS_MINIPROJECT.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) -lm

run: $(TARGET)
	./$(TARGET) data/sample_threads.csv

clean:
	rm -f $(TARGET)

.PHONY: run clean