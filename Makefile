CC = cc 
TARGET = target

IMFS_BIN = $(TARGET)/imfs
IMFS_OBJ = $(TARGET)/imfs.o
TEST_BIN = $(TARGET)/tests

IMFS_SRC = imfs.c
TEST_SRC = tests.c

$(TARGET):
	mkdir -p $(TARGET)

imfs: $(TARGET) $(IMFS_SRC)
	$(CC) $(IMFS_SRC) -o $(IMFS_BIN)

debug: $(TARGET) $(IMFS_SRC)
	$(CC) $(IMFS_SRC) -g -o $(IMFS_BIN)

lib: $(TARGET) $(IMFS_SRC)
	$(CC) $(CFLAGS) -DLIB -c $(IMFS_SRC) -o $(IMFS_OBJ)

tests: $(TARGET) $(TEST_SRC) $(IMFS_SRC)
	$(CC) -DLIB $(TEST_SRC) $(IMFS_SRC) -o $(TEST_BIN)
	@$(TEST_BIN)

test-%: $(TARGET) $(TEST_SRC) $(IMFS_SRC)
	$(CC) -DLIB $(TEST_SRC) $(IMFS_SRC) -o $(TEST_BIN)
	@$(TEST_BIN) -g "$*"

test: tests
imfs: imfs

clean:
	rm -rf $(TARGET)
