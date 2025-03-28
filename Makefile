CC = cc
CFLAGS = -Wall -O2
LIBS = -lreadline -lm
TARGET = dotmis
SRC = DotMis.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
