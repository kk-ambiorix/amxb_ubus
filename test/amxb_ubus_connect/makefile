include ../test_defines.mk

TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

include ../test_targets.mk

$(TEST_SO): $(SO_OBJECTS)
	$(CC) -Wl,-soname,$(TARGET)-$(VMAJOR).so -o $@ $(OBJECTS) $(LDFLAGS) -shared -fPIC
