CC = cc 
TARGET = target

FLAGS = -D_GNU_SOURCE

IMFS_BIN = $(TARGET)/imfs
IMFS_OBJ = $(TARGET)/imfs.o
TEST_BIN = $(TARGET)/tests/fstest

IMFS_SRC = imfs.c
TEST_SRC = tests.c

PJD_SRC = ./pjdfstest/pjdfstest.c 
PJD_BIN = $(TARGET)/fstest 

PJD_FST = ./pjdfstest/fstest.c 
FST_BIN = ./pjdfstest/tests/fstestrun

$(TARGET):
	mkdir -p $(TARGET)

imfs: $(TARGET) $(IMFS_SRC)
	$(CC) $(FLAGS) $(IMFS_SRC) -o $(IMFS_BIN)

debug: $(TARGET) $(IMFS_SRC)
	$(CC) $(FLAGS) $(IMFS_SRC) -g -o $(IMFS_BIN)

lib: $(TARGET) $(IMFS_SRC)
	$(CC) $(FLAGS) $(CFLAGS) -DLIB -c $(IMFS_SRC) -o $(IMFS_OBJ)

tests: $(TARGET) $(TEST_SRC) $(IMFS_SRC)
	$(CC) -DLIB $(FLAGS) $(TEST_SRC) $(IMFS_SRC) -o $(TEST_BIN)
	@$(TEST_BIN)

test-%: $(TARGET) $(TEST_SRC) $(IMFS_SRC)
	$(CC) -DLIB $(FLAGS) $(TEST_SRC) $(IMFS_SRC) -o $(TEST_BIN)
	@$(TEST_BIN) -g "$*"

pjdfstest: $(TARGET) $(PJD_SRC) $(PJD_FST) $(IMFS_SRC)
	$(CC) -DLIB $(IMFS_SRC) $(PJD_SRC) -o $(PJD_BIN) 
	$(CC) $(PJD_FST) -o $(FST_BIN)

test: tests
imfs: imfs

clean:
	rm -rf $(TARGET)
