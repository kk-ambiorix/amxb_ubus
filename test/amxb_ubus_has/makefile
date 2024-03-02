include ../test_defines.mk

TEST_SO = ../mod-amxb-test-ubus.so

TARGET = run_test
SO_SOURCE = $(SOURCES)
SO_OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SO_SOURCES:.c=.o)))
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

include ../test_targets.mk

$(TEST_SO): $(SO_OBJECTS)
	$(CC) -Wl,-soname,$(TARGET)-$(VMAJOR).so -o $@ $(OBJECTS) $(LDFLAGS) -shared -fPIC
