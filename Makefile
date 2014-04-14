CC = clang
CXX = clang++
CFLAGS += -Wall -g
CXXFLAGS += $(CFLAGS) -std=c++11
LIBS += $(shell pkg-config --libs wayland-client)
INCLUDES += $(shell pkg-config --cflags wayland-client)
OBJS = os-compatibility.o simple-client.o
TARGET = simple-client

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)