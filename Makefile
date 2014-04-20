CFLAGS += -Wall -g
LIBS += $(shell pkg-config --libs wayland-client)
INCLUDES += $(shell pkg-config --cflags wayland-client)
OBJS = os-compatibility.o simple-client.o
TARGET = simple-client

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
