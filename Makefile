CC = gcc
CFLAGS = -Wall -Wextra
PDC_DIR = deps/PDCurses
PDC_CFLAGS = -O2 -I$(PDC_DIR) -DHAVE_NO_INFOEX
SRC = graphics_editor.c
TARGET = graphics_editor

PDC_CORE = addch addchstr addstr attr beep bkgd border clear color delch \
	deleteln getch getstr getyx inch inchstr initscr inopts insch insstr \
	instr kernel keyname mouse move outopts overlay pad panel printw refresh \
	scanw scr_dump scroll slk termattr touch util window debug

PDC_WIN = pdcclip pdcdisp pdcgetsc pdckbd pdcscrn pdcsetsc pdcutil

PDC_SRCS = $(addprefix $(PDC_DIR)/pdcurses/,$(addsuffix .c,$(PDC_CORE))) \
	$(addprefix $(PDC_DIR)/wincon/,$(addsuffix .c,$(PDC_WIN)))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC) $(PDC_SRCS)
	$(CC) $(CFLAGS) $(PDC_CFLAGS) $(SRC) $(PDC_SRCS) -o $(TARGET) -lm

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
