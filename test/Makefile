TARGET = FWTP_test

BUILD_DIR = build

OPT = -O3

C_SOURCES =  \
main.c \
../fwtp.c

C_INCLUDES = \
-I../

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall

CC = gcc

all: $(BUILD_DIR)/$(TARGET)

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) $< -o $@	
	
$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile
	$(CC) $(OBJECTS) -o $@
	
$(BUILD_DIR):
	mkdir $@		

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
 