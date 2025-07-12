# === Config ===
TARGET := build/main
SRCS := main.c network.c sigchld.c handle_connection.c

# === Auto-Derived ===
SRC_DIR := src
OBJ_DIR := build
OBJS := $(addprefix $(OBJ_DIR)/, $(SRCS:.c=.o))
DEPS := $(OBJS:.o=.d)
CC := gcc
CFLAGS := -Wall -Wextra -std=gnu11 -Iinclude -MMD -MP

# === Rules ===
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

run:
	./$(TARGET)

# auto-include dependency files
-include $(DEPS)

.PHONY: all clean
