#include <curses.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 40
#define HEIGHT 20
#define MAX_OBJECTS 100

typedef enum {
  SHAPE_CIRCLE,
  SHAPE_RECTANGLE,
  SHAPE_LINE,
  SHAPE_TRIANGLE
} ShapeType;

typedef struct {
  ShapeType type;
  int id;
  bool is_filled;        /* NEW: fill mode support */
  short color_pair_id;   /* NEW: per-shape color support */
  union {
    struct {
      int cx, cy, radius;
    } circle;
    struct {
      int x1, y1, x2, y2;
    } rectangle;
    struct {
      int x1, y1, x2, y2;
    } line;
    struct {
      int x1, y1, x2, y2, x3, y3;
    } triangle;
  } data;
} Shape;

typedef enum {
  MENU_ADD_CIRCLE,
  MENU_ADD_RECT,
  MENU_ADD_LINE,
  MENU_ADD_TRIANGLE,
  MENU_DELETE,
  MENU_MODIFY,
  MENU_CLEAR,
  MENU_ZOOM_IN,
  MENU_ZOOM_OUT,
  MENU_EXIT,
  MENU_COUNT
} MenuItem;

static const char *MENU_LABELS[MENU_COUNT] = {
    "Add Circle",    "Add Rectangle", "Add Line",
    "Add Triangle",  "Delete Shape",  "Modify Shape",
    "Clear Canvas",   "Zoom In",       "Zoom Out",
    "Exit"};

static char canvas[HEIGHT][WIDTH];
static short canvasColor[HEIGHT][WIDTH];
static Shape objects[MAX_OBJECTS];
static int objectCount = 0;
static int nextId = 1;

static WINDOW *canvasWin = NULL;
static WINDOW *menuWin = NULL;
static WINDOW *infoWin = NULL;
static WINDOW *statusWin = NULL;
static float zoomScale = 1.0f; /* NEW: canvas zoom scale */
static short activeColorPair = 4; /* NEW: current color for new shapes */
static int selectedShapeId = -1; /* NEW: currently selected shape */

enum {
  COLOR_CHOICE_WHITE,
  COLOR_CHOICE_RED,
  COLOR_CHOICE_GREEN,
  COLOR_CHOICE_BLUE,
  COLOR_CHOICE_YELLOW,
  COLOR_CHOICE_CYAN,
  COLOR_CHOICE_MAGENTA
};

static const char *COLOR_NAMES[] = {
    "White", "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta"};

static bool isPrimaryMouseClick(const MEVENT *event) {
  return (event->bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED |
                           BUTTON1_DOUBLE_CLICKED)) != 0;
}

static void initCanvas(void) {
  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < WIDTH; j++) {
      canvas[i][j] = ' ';
      canvasColor[i][j] = 0;
    }
  }
}

static void setPixel(int x, int y, short colorPair) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    canvas[y][x] = '*';
    canvasColor[y][x] = colorPair;
  }
}

static int clampToRange(int value, int minVal, int maxVal) {
  if (value < minVal)
    return minVal;
  if (value > maxVal)
    return maxVal;
  return value;
}

static int scaleCoord(int value, int center, float factor, int maxValue) {
  int scaled = (int)lrintf((float)center + ((float)value - (float)center) * factor);
  return clampToRange(scaled, 0, maxValue);
}

static void drawHorizontalLine(int x1, int x2, int y, short colorPair) {
  if (x1 > x2) {
    int tmp = x1;
    x1 = x2;
    x2 = tmp;
  }
  for (int x = x1; x <= x2; x++)
    setPixel(x, y, colorPair);
}

static int pointInTriangle(int x, int y, int x1, int y1, int x2, int y2,
                           int x3, int y3) {
  int c1 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
  int c2 = (x - x2) * (y3 - y2) - (y - y2) * (x3 - x2);
  int c3 = (x - x3) * (y1 - y3) - (y - y3) * (x1 - x3);
  return (c1 >= 0 && c2 >= 0 && c3 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0);
}

static void drawCirclePixels(int cx, int cy, int radius, short colorPair,
                             bool filled) {
  if (radius < 0)
    return;

  if (filled) {
    for (int y = cy - radius; y <= cy + radius; y++) {
      if (y < 0 || y >= HEIGHT)
        continue;
      double dy = (double)(y - cy);
      double dx = sqrt((double)(radius * radius - dy * dy));
      int xStart = (int)floor(cx - dx);
      int xEnd = (int)ceil(cx + dx);
      drawHorizontalLine(clampToRange(xStart, 0, WIDTH - 1),
                         clampToRange(xEnd, 0, WIDTH - 1), y, colorPair);
    }
    return;
  }

  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;
  while (y >= x) {
    setPixel(cx + x, cy + y, colorPair);
    setPixel(cx - x, cy + y, colorPair);
    setPixel(cx + x, cy - y, colorPair);
    setPixel(cx - x, cy - y, colorPair);
    setPixel(cx + y, cy + x, colorPair);
    setPixel(cx - y, cy + x, colorPair);
    setPixel(cx + y, cy - x, colorPair);
    setPixel(cx - y, cy - x, colorPair);
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else {
      d = d + 4 * x + 6;
    }
  }
}

static void drawRectanglePixels(int x1, int y1, int x2, int y2,
                                short colorPair, bool filled) {
  if (filled) {
    int left = clampToRange((x1 < x2) ? x1 : x2, 0, WIDTH - 1);
    int right = clampToRange((x1 < x2) ? x2 : x1, 0, WIDTH - 1);
    int top = clampToRange((y1 < y2) ? y1 : y2, 0, HEIGHT - 1);
    int bottom = clampToRange((y1 < y2) ? y2 : y1, 0, HEIGHT - 1);
    for (int y = top; y <= bottom; y++) {
      for (int x = left; x <= right; x++)
        setPixel(x, y, colorPair);
    }
    return;
  }

  for (int x = x1; x <= x2; x++) {
    setPixel(x, y1, colorPair);
    setPixel(x, y2, colorPair);
  }
  for (int y = y1; y <= y2; y++) {
    setPixel(x1, y, colorPair);
    setPixel(x2, y, colorPair);
  }
}

static void drawLinePixels(int x1, int y1, int x2, int y2, short colorPair) {
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    setPixel(x1, y1, colorPair);
    if (x1 == x2 && y1 == y2)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

static void drawTrianglePixels(int x1, int y1, int x2, int y2, int x3, int y3,
                               short colorPair, bool filled) {
  if (filled) {
    int minX = clampToRange((x1 < x2 ? x1 : x2) < x3 ? (x1 < x2 ? x1 : x2) : x3,
                             0, WIDTH - 1);
    int maxX = clampToRange((x1 > x2 ? x1 : x2) > x3 ? (x1 > x2 ? x1 : x2) : x3,
                             0, WIDTH - 1);
    int minY = clampToRange((y1 < y2 ? y1 : y2) < y3 ? (y1 < y2 ? y1 : y2) : y3,
                             0, HEIGHT - 1);
    int maxY = clampToRange((y1 > y2 ? y1 : y2) > y3 ? (y1 > y2 ? y1 : y2) : y3,
                             0, HEIGHT - 1);
    for (int y = minY; y <= maxY; y++) {
      for (int x = minX; x <= maxX; x++) {
        if (pointInTriangle(x, y, x1, y1, x2, y2, x3, y3))
          setPixel(x, y, colorPair);
      }
    }
    return;
  }

  drawLinePixels(x1, y1, x2, y2, colorPair);
  drawLinePixels(x2, y2, x3, y3, colorPair);
  drawLinePixels(x3, y3, x1, y1, colorPair);
}

static void redrawCanvas(void) {
  initCanvas();
  float factor = zoomScale;
  if (factor < 0.25f)
    factor = 0.25f;

  for (int i = 0; i < objectCount; i++) {
    Shape *s = &objects[i];
    switch (s->type) {
    case SHAPE_CIRCLE: {
      int cx = scaleCoord(s->data.circle.cx, WIDTH / 2, factor, WIDTH - 1);
      int cy = scaleCoord(s->data.circle.cy, HEIGHT / 2, factor, HEIGHT - 1);
      int radius = (int)lrintf((float)s->data.circle.radius * factor);
      if (radius < 1)
        radius = 1;
      drawCirclePixels(cx, cy, radius, s->color_pair_id, s->is_filled);
      break;
    }
    case SHAPE_RECTANGLE: {
      int x1 = scaleCoord(s->data.rectangle.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.rectangle.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.rectangle.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.rectangle.y2, HEIGHT / 2, factor, HEIGHT - 1);
      drawRectanglePixels(x1, y1, x2, y2, s->color_pair_id, s->is_filled);
      break;
    }
    case SHAPE_LINE: {
      int x1 = scaleCoord(s->data.line.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.line.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.line.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.line.y2, HEIGHT / 2, factor, HEIGHT - 1);
      drawLinePixels(x1, y1, x2, y2, s->color_pair_id);
      break;
    }
    case SHAPE_TRIANGLE: {
      int x1 = scaleCoord(s->data.triangle.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.triangle.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.triangle.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.triangle.y2, HEIGHT / 2, factor, HEIGHT - 1);
      int x3 = scaleCoord(s->data.triangle.x3, WIDTH / 2, factor, WIDTH - 1);
      int y3 = scaleCoord(s->data.triangle.y3, HEIGHT / 2, factor, HEIGHT - 1);
      drawTrianglePixels(x1, y1, x2, y2, x3, y3, s->color_pair_id,
                         s->is_filled);
      break;
    }
    }
  }
}

static int addCircle(int cx, int cy, int radius, short colorPair,
                     bool filled) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_CIRCLE;
  s->id = nextId++;
  s->color_pair_id = (colorPair > 0) ? colorPair : 1;
  s->is_filled = filled;
  s->data.circle.cx = cx;
  s->data.circle.cy = cy;
  s->data.circle.radius = radius;
  redrawCanvas();
  return s->id;
}

static int addRectangle(int x1, int y1, int x2, int y2, short colorPair,
                        bool filled) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_RECTANGLE;
  s->id = nextId++;
  s->color_pair_id = (colorPair > 0) ? colorPair : 1;
  s->is_filled = filled;
  s->data.rectangle.x1 = x1;
  s->data.rectangle.y1 = y1;
  s->data.rectangle.x2 = x2;
  s->data.rectangle.y2 = y2;
  redrawCanvas();
  return s->id;
}

static int addLine(int x1, int y1, int x2, int y2, short colorPair,
                   bool filled) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_LINE;
  s->id = nextId++;
  s->color_pair_id = (colorPair > 0) ? colorPair : 1;
  s->is_filled = filled;
  s->data.line.x1 = x1;
  s->data.line.y1 = y1;
  s->data.line.x2 = x2;
  s->data.line.y2 = y2;
  redrawCanvas();
  return s->id;
}

static int addTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                       short colorPair, bool filled) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_TRIANGLE;
  s->id = nextId++;
  s->color_pair_id = (colorPair > 0) ? colorPair : 1;
  s->is_filled = filled;
  s->data.triangle.x1 = x1;
  s->data.triangle.y1 = y1;
  s->data.triangle.x2 = x2;
  s->data.triangle.y2 = y2;
  s->data.triangle.x3 = x3;
  s->data.triangle.y3 = y3;
  redrawCanvas();
  return s->id;
}

static int deleteObject(int id) {
  int found = -1;
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id) {
      found = i;
      break;
    }
  }
  if (found == -1)
    return 0;

  for (int i = found; i < objectCount - 1; i++) {
    objects[i] = objects[i + 1];
  }
  objectCount--;
  redrawCanvas();
  return 1;
}

static int modifyCircle(int id, int cx, int cy, int radius, short colorPair,
                        bool filled) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_CIRCLE) {
      objects[i].data.circle.cx = cx;
      objects[i].data.circle.cy = cy;
      objects[i].data.circle.radius = radius;
      objects[i].color_pair_id = (colorPair > 0) ? colorPair : 1;
      objects[i].is_filled = filled;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

static int modifyRectangle(int id, int x1, int y1, int x2, int y2,
                           short colorPair, bool filled) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_RECTANGLE) {
      objects[i].data.rectangle.x1 = x1;
      objects[i].data.rectangle.y1 = y1;
      objects[i].data.rectangle.x2 = x2;
      objects[i].data.rectangle.y2 = y2;
      objects[i].color_pair_id = (colorPair > 0) ? colorPair : 1;
      objects[i].is_filled = filled;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

static int modifyLine(int id, int x1, int y1, int x2, int y2,
                      short colorPair, bool filled) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_LINE) {
      objects[i].data.line.x1 = x1;
      objects[i].data.line.y1 = y1;
      objects[i].data.line.x2 = x2;
      objects[i].data.line.y2 = y2;
      objects[i].color_pair_id = (colorPair > 0) ? colorPair : 1;
      objects[i].is_filled = filled;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

static int modifyTriangle(int id, int x1, int y1, int x2, int y2, int x3,
                          int y3, short colorPair, bool filled) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_TRIANGLE) {
      objects[i].data.triangle.x1 = x1;
      objects[i].data.triangle.y1 = y1;
      objects[i].data.triangle.x2 = x2;
      objects[i].data.triangle.y2 = y2;
      objects[i].data.triangle.x3 = x3;
      objects[i].data.triangle.y3 = y3;
      objects[i].color_pair_id = (colorPair > 0) ? colorPair : 1;
      objects[i].is_filled = filled;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

static const char *colorNameForPair(short pairId);

static void shapeLabel(const Shape *s, char *buf, size_t buflen) {
  const char *fillText = s->is_filled ? "filled" : "wire";
  const char *colorText = colorNameForPair(s->color_pair_id);

  switch (s->type) {
  case SHAPE_CIRCLE:
    snprintf(buf, buflen,
             "ID %d: Circle (%s, %s, center=%d,%d, r=%d)", s->id,
             colorText, fillText, s->data.circle.cx, s->data.circle.cy,
             s->data.circle.radius);
    break;
  case SHAPE_RECTANGLE:
    snprintf(buf, buflen,
             "ID %d: Rect (%s, %s, %d,%d)-(%d,%d)", s->id, colorText,
             fillText, s->data.rectangle.x1, s->data.rectangle.y1,
             s->data.rectangle.x2, s->data.rectangle.y2);
    break;
  case SHAPE_LINE:
    snprintf(buf, buflen,
             "ID %d: Line (%s, %s, %d,%d)-(%d,%d)", s->id, colorText,
             fillText, s->data.line.x1, s->data.line.y1, s->data.line.x2,
             s->data.line.y2);
    break;
  case SHAPE_TRIANGLE:
    snprintf(buf, buflen,
             "ID %d: Triangle (%s, %s, %d,%d)(%d,%d)(%d,%d)", s->id,
             colorText, fillText, s->data.triangle.x1, s->data.triangle.y1,
             s->data.triangle.x2, s->data.triangle.y2, s->data.triangle.x3,
             s->data.triangle.y3);
    break;
  }
}

static void drawCanvasWindow(void) {
  werase(canvasWin);
  box(canvasWin, 0, 0);
  mvwprintw(canvasWin, 0, 2, " Canvas ");

  wattron(canvasWin, COLOR_PAIR(2));
  mvwprintw(canvasWin, 1, 2, " +");
  for (int j = 0; j < WIDTH; j++) {
    waddstr(canvasWin, "--");
  }
  waddch(canvasWin, '+');

  for (int i = 0; i < HEIGHT; i++) {
    mvwprintw(canvasWin, 2 + i, 2, "%2d|", i);
    for (int j = 0; j < WIDTH; j++) {
      short pair = canvasColor[i][j];
      if (pair > 0)
        wattron(canvasWin, COLOR_PAIR(pair));
      wprintw(canvasWin, "%c ", canvas[i][j]);
      if (pair > 0)
        wattroff(canvasWin, COLOR_PAIR(pair));
    }
    waddch(canvasWin, '|');
  }

  mvwprintw(canvasWin, 2 + HEIGHT, 2, "  +");
  for (int j = 0; j < WIDTH; j++) {
    waddstr(canvasWin, "--");
  }
  waddch(canvasWin, '+');

  mvwprintw(canvasWin, 3 + HEIGHT, 4, " ");
  for (int j = 0; j < WIDTH; j += 5) {
    wprintw(canvasWin, "%-10d", j);
  }
  wattroff(canvasWin, COLOR_PAIR(2));
  wrefresh(canvasWin);
}

static void drawMenuWindow(int selected) {
  werase(menuWin);
  box(menuWin, 0, 0);
  mvwprintw(menuWin, 0, 2, " Menu ");

  for (int i = 0; i < MENU_COUNT; i++) {
    if (i == selected) {
      wattron(menuWin, A_REVERSE | COLOR_PAIR(1));
      mvwprintw(menuWin, 1 + i, 2, "> %-22s", MENU_LABELS[i]);
      wattroff(menuWin, A_REVERSE | COLOR_PAIR(1));
    } else {
      mvwprintw(menuWin, 1 + i, 2, "  %-22s", MENU_LABELS[i]);
    }
  }

  wrefresh(menuWin);
}

static void drawInfoWindow(void) {
  werase(infoWin);
  box(infoWin, 0, 0);
  mvwprintw(infoWin, 0, 2, " Objects ");

  int maxLines = getmaxy(infoWin) - 2;
  if (objectCount == 0) {
    mvwprintw(infoWin, 1, 2, "(none)");
  } else {
    for (int i = 0; i < objectCount && i < maxLines; i++) {
      char line[128];
      shapeLabel(&objects[i], line, sizeof(line));
      mvwprintw(infoWin, 1 + i, 2, "%.*s",
                getmaxx(infoWin) - 4, line);
    }
    if (objectCount > maxLines) {
      mvwprintw(infoWin, 1 + maxLines, 2, "... %d more",
                objectCount - maxLines);
    }
  }
  wrefresh(infoWin);
}

static void drawStatusWindow(const char *message) {
  werase(statusWin);
  wattron(statusWin, COLOR_PAIR(3));
  mvwprintw(statusWin, 0, 1, "%s", message);
  wattroff(statusWin, COLOR_PAIR(3));
  wrefresh(statusWin);
}

static void layoutWindows(void) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  if (canvasWin)
    delwin(canvasWin);
  if (menuWin)
    delwin(menuWin);
  if (infoWin)
    delwin(infoWin);
  if (statusWin)
    delwin(statusWin);

  int canvasH = HEIGHT + 6;
  int canvasW = WIDTH * 2 + 8;
  int menuW = 28;
  int infoH = 8;
  int statusH = 1;

  if (cols < canvasW + menuW + 2)
    canvasW = cols - menuW - 2;
  if (canvasW < 30)
    canvasW = 30;
  if (rows < canvasH + infoH + statusH + 1)
    canvasH = rows - infoH - statusH - 2;
  if (canvasH < 8)
    canvasH = 8;

  canvasWin = newwin(canvasH, canvasW, 1, 1);
  menuWin = newwin(canvasH, menuW, 1, canvasW + 1);
  infoWin = newwin(infoH, canvasW + menuW, canvasH + 1, 1);
  statusWin = newwin(statusH, canvasW + menuW, canvasH + infoH + 1, 1);

  keypad(menuWin, TRUE);
  keypad(canvasWin, TRUE);
  keypad(infoWin, TRUE);
  wtimeout(canvasWin, 100);
  wtimeout(menuWin, 100);
  wtimeout(infoWin, 100);
}

static void refreshAll(int menuSelected, const char *status) {
  drawCanvasWindow();
  drawMenuWindow(menuSelected);
  drawInfoWindow();
  drawStatusWindow(status);
}

static int readIntInDialog(WINDOW *dlg, int row, const char *label, int minVal,
                           int maxVal, int current) {
  char input[32];
  snprintf(input, sizeof(input), "%d", current);
  curs_set(1);

  while (1) {
    mvwprintw(dlg, row, 2, "%-28s", label);
    wclrtoeol(dlg);
    mvwprintw(dlg, row, 30, "[%s] ", input);
    wclrtoeol(dlg);
    wrefresh(dlg);

    int ch = wgetch(dlg);
    if (ch == 27) {
      curs_set(0);
      return -1;
    }
    if (ch == '\n' || ch == KEY_ENTER) {
      int value;
      if (sscanf(input, "%d", &value) == 1 && value >= minVal &&
          value <= maxVal) {
        curs_set(0);
        return value;
      }
      beep();
      continue;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      size_t len = strlen(input);
      if (len > 0)
        input[len - 1] = '\0';
      continue;
    }
    if (ch >= '0' && ch <= '9') {
      size_t len = strlen(input);
      if (len < sizeof(input) - 2) {
        input[len] = (char)ch;
        input[len + 1] = '\0';
      }
    }
  }
}

static int colorChoiceForPair(short colorPair) {
  if (colorPair >= 4 && colorPair <= 10)
    return colorPair - 4;
  return 0;
}

static short promptColorChoice(WINDOW *dlg, int row, short currentPair) {
  int choice = readIntInDialog(dlg, row,
                               "Color (0 White..6 Magenta)", 0, 6,
                               colorChoiceForPair(currentPair));
  if (choice < 0)
    return -1;
  return (short)(choice + 4);
}

static short nextColorPair(short currentPair) {
  int index = (currentPair >= 4 && currentPair <= 10) ? (currentPair - 4) : 0;
  index = (index + 1) % 7;
  return (short)(index + 4);
}

static const char *colorNameForPair(short pairId) {
  if (pairId >= 4 && pairId <= 10)
    return COLOR_NAMES[pairId - 4];
  return COLOR_NAMES[0];
}

static int promptFilledChoice(WINDOW *dlg, int row, bool currentFilled) {
  return readIntInDialog(dlg, row, "Filled? (1 Yes, 0 No)", 0, 1,
                         currentFilled ? 1 : 0);
}

static void showMessageDialog(const char *title, const char *message) {
  int dlgH = 7;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " %s ", title);
  mvwprintw(dlg, 2, 2, "%.*s", dlgW - 4, message);
  mvwprintw(dlg, dlgH - 2, 2, "Press Enter to close");
  wrefresh(dlg);

  while (1) {
    int ch = wgetch(dlg);
    if (ch == '\n' || ch == KEY_ENTER || ch == 27)
      break;
  }
  delwin(dlg);
  touchwin(stdscr);
  refresh();
}

static int pickObjectDialog(const char *title) {
  if (objectCount == 0)
    return -1;

  int selected = 0;
  int dlgH = objectCount + 6;
  if (dlgH > getmaxy(stdscr) - 4)
    dlgH = getmaxy(stdscr) - 4;
  int dlgW = 56;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);

  while (1) {
    werase(dlg);
    box(dlg, 0, 0);
    mvwprintw(dlg, 0, 2, " %s ", title);
    mvwprintw(dlg, 1, 2, "Up/Down select  Enter confirm  Esc cancel");

    int visible = dlgH - 5;
    for (int i = 0; i < visible && i < objectCount; i++) {
      char line[128];
      shapeLabel(&objects[i], line, sizeof(line));
      if (i == selected) {
        wattron(dlg, A_REVERSE);
        mvwprintw(dlg, 3 + i, 2, "> %.*s", dlgW - 6, line);
        wattroff(dlg, A_REVERSE);
      } else {
        mvwprintw(dlg, 3 + i, 2, "  %.*s", dlgW - 6, line);
      }
    }
    wrefresh(dlg);

    int ch = wgetch(dlg);
    if (ch == 27)
      break;
    if (ch == KEY_UP)
      selected = (selected - 1 + objectCount) % objectCount;
    else if (ch == KEY_DOWN)
      selected = (selected + 1) % objectCount;
    else if (ch == '\n' || ch == KEY_ENTER) {
      int id = objects[selected].id;
      delwin(dlg);
      touchwin(stdscr);
      refresh();
      return id;
    }
  }

  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return -1;
}

static int dialogAddCircle(void) {
  int dlgH = 14;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " Add Circle ");
  wrefresh(dlg);

  int cx = readIntInDialog(dlg, 2, "Center X (0-39)", 0, WIDTH - 1, 10);
  if (cx < 0)
    goto cleanup;
  int cy = readIntInDialog(dlg, 3, "Center Y (0-19)", 0, HEIGHT - 1, 5);
  if (cy < 0)
    goto cleanup;
  int r = readIntInDialog(dlg, 4, "Radius (1-20)", 1, 20, 5);
  if (r < 0)
    goto cleanup;
  short colorPair = promptColorChoice(dlg, 6, 1);
  if (colorPair < 0)
    goto cleanup;
  int filled = promptFilledChoice(dlg, 7, false);
  if (filled < 0)
    goto cleanup;

  int id = addCircle(cx, cy, r, colorPair, filled == 1);
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return id;

cleanup:
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return -1;
}

static int dialogAddRectangle(void) {
  int dlgH = 16;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " Add Rectangle ");
  wrefresh(dlg);

  int x1 = readIntInDialog(dlg, 2, "Top-left X (0-39)", 0, WIDTH - 1, 5);
  if (x1 < 0)
    goto cleanup;
  int y1 = readIntInDialog(dlg, 3, "Top-left Y (0-19)", 0, HEIGHT - 1, 5);
  if (y1 < 0)
    goto cleanup;
  int x2 = readIntInDialog(dlg, 4, "Bottom-right X (0-39)", 0, WIDTH - 1, 25);
  if (x2 < 0)
    goto cleanup;
  int y2 = readIntInDialog(dlg, 5, "Bottom-right Y (0-19)", 0, HEIGHT - 1, 12);
  if (y2 < 0)
    goto cleanup;
  short colorPair = promptColorChoice(dlg, 7, 1);
  if (colorPair < 0)
    goto cleanup;
  int filled = promptFilledChoice(dlg, 8, false);
  if (filled < 0)
    goto cleanup;

  int id = addRectangle(x1, y1, x2, y2, colorPair, filled == 1);
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return id;

cleanup:
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return -1;
}

static int dialogAddLine(void) {
  int dlgH = 15;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " Add Line ");
  wrefresh(dlg);

  int x1 = readIntInDialog(dlg, 2, "Start X (0-39)", 0, WIDTH - 1, 2);
  if (x1 < 0)
    goto cleanup;
  int y1 = readIntInDialog(dlg, 3, "Start Y (0-19)", 0, HEIGHT - 1, 2);
  if (y1 < 0)
    goto cleanup;
  int x2 = readIntInDialog(dlg, 4, "End X (0-39)", 0, WIDTH - 1, 30);
  if (x2 < 0)
    goto cleanup;
  int y2 = readIntInDialog(dlg, 5, "End Y (0-19)", 0, HEIGHT - 1, 10);
  if (y2 < 0)
    goto cleanup;
  short colorPair = promptColorChoice(dlg, 7, 1);
  if (colorPair < 0)
    goto cleanup;
  int filled = promptFilledChoice(dlg, 8, false);
  if (filled < 0)
    goto cleanup;

  int id = addLine(x1, y1, x2, y2, colorPair, filled == 1);
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return id;

cleanup:
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return -1;
}

static int dialogAddTriangle(void) {
  int dlgH = 18;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " Add Triangle ");
  wrefresh(dlg);

  int x1 = readIntInDialog(dlg, 2, "Vertex 1 X (0-39)", 0, WIDTH - 1, 5);
  if (x1 < 0)
    goto cleanup;
  int y1 = readIntInDialog(dlg, 3, "Vertex 1 Y (0-19)", 0, HEIGHT - 1, 15);
  if (y1 < 0)
    goto cleanup;
  int x2 = readIntInDialog(dlg, 4, "Vertex 2 X (0-39)", 0, WIDTH - 1, 20);
  if (x2 < 0)
    goto cleanup;
  int y2 = readIntInDialog(dlg, 5, "Vertex 2 Y (0-19)", 0, HEIGHT - 1, 5);
  if (y2 < 0)
    goto cleanup;
  int x3 = readIntInDialog(dlg, 6, "Vertex 3 X (0-39)", 0, WIDTH - 1, 10);
  if (x3 < 0)
    goto cleanup;
  int y3 = readIntInDialog(dlg, 7, "Vertex 3 Y (0-19)", 0, HEIGHT - 1, 18);
  if (y3 < 0)
    goto cleanup;
  short colorPair = promptColorChoice(dlg, 9, 1);
  if (colorPair < 0)
    goto cleanup;
  int filled = promptFilledChoice(dlg, 10, false);
  if (filled < 0)
    goto cleanup;

  int id = addTriangle(x1, y1, x2, y2, x3, y3, colorPair, filled == 1);
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return id;

cleanup:
  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return -1;
}

static Shape *findShapeById(int id) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id)
      return &objects[i];
  }
  return NULL;
}

static int dialogModifyShape(int id) {
  Shape *s = findShapeById(id);
  if (!s)
    return 0;

  int dlgH = 18;
  int dlgW = 52;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  box(dlg, 0, 0);
  mvwprintw(dlg, 0, 2, " Modify Shape (ID %d) ", id);
  wrefresh(dlg);

  int ok = 0;
  switch (s->type) {
  case SHAPE_CIRCLE: {
    int cx = readIntInDialog(dlg, 2, "Center X (0-39)", 0, WIDTH - 1,
                             s->data.circle.cx);
    if (cx < 0)
      break;
    int cy = readIntInDialog(dlg, 3, "Center Y (0-19)", 0, HEIGHT - 1,
                             s->data.circle.cy);
    if (cy < 0)
      break;
    int r = readIntInDialog(dlg, 4, "Radius (1-20)", 1, 20,
                            s->data.circle.radius);
    if (r < 0)
      break;
    short colorPair = promptColorChoice(dlg, 6, s->color_pair_id);
    if (colorPair < 0)
      break;
    int filled = promptFilledChoice(dlg, 7, s->is_filled);
    if (filled < 0)
      break;
    ok = modifyCircle(id, cx, cy, r, colorPair, filled == 1);
    break;
  }
  case SHAPE_RECTANGLE: {
    int x1 = readIntInDialog(dlg, 2, "Top-left X (0-39)", 0, WIDTH - 1,
                             s->data.rectangle.x1);
    if (x1 < 0)
      break;
    int y1 = readIntInDialog(dlg, 3, "Top-left Y (0-19)", 0, HEIGHT - 1,
                             s->data.rectangle.y1);
    if (y1 < 0)
      break;
    int x2 = readIntInDialog(dlg, 4, "Bottom-right X (0-39)", 0, WIDTH - 1,
                             s->data.rectangle.x2);
    if (x2 < 0)
      break;
    int y2 = readIntInDialog(dlg, 5, "Bottom-right Y (0-19)", 0, HEIGHT - 1,
                             s->data.rectangle.y2);
    if (y2 < 0)
      break;
    short colorPair = promptColorChoice(dlg, 7, s->color_pair_id);
    if (colorPair < 0)
      break;
    int filled = promptFilledChoice(dlg, 8, s->is_filled);
    if (filled < 0)
      break;
    ok = modifyRectangle(id, x1, y1, x2, y2, colorPair, filled == 1);
    break;
  }
  case SHAPE_LINE: {
    int x1 = readIntInDialog(dlg, 2, "Start X (0-39)", 0, WIDTH - 1,
                             s->data.line.x1);
    if (x1 < 0)
      break;
    int y1 = readIntInDialog(dlg, 3, "Start Y (0-19)", 0, HEIGHT - 1,
                             s->data.line.y1);
    if (y1 < 0)
      break;
    int x2 = readIntInDialog(dlg, 4, "End X (0-39)", 0, WIDTH - 1,
                             s->data.line.x2);
    if (x2 < 0)
      break;
    int y2 = readIntInDialog(dlg, 5, "End Y (0-19)", 0, HEIGHT - 1,
                             s->data.line.y2);
    if (y2 < 0)
      break;
    short colorPair = promptColorChoice(dlg, 7, s->color_pair_id);
    if (colorPair < 0)
      break;
    int filled = promptFilledChoice(dlg, 8, s->is_filled);
    if (filled < 0)
      break;
    ok = modifyLine(id, x1, y1, x2, y2, colorPair, filled == 1);
    break;
  }
  case SHAPE_TRIANGLE: {
    int x1 = readIntInDialog(dlg, 2, "Vertex 1 X (0-39)", 0, WIDTH - 1,
                             s->data.triangle.x1);
    if (x1 < 0)
      break;
    int y1 = readIntInDialog(dlg, 3, "Vertex 1 Y (0-19)", 0, HEIGHT - 1,
                             s->data.triangle.y1);
    if (y1 < 0)
      break;
    int x2 = readIntInDialog(dlg, 4, "Vertex 2 X (0-39)", 0, WIDTH - 1,
                             s->data.triangle.x2);
    if (x2 < 0)
      break;
    int y2 = readIntInDialog(dlg, 5, "Vertex 2 Y (0-19)", 0, HEIGHT - 1,
                             s->data.triangle.y2);
    if (y2 < 0)
      break;
    int x3 = readIntInDialog(dlg, 6, "Vertex 3 X (0-39)", 0, WIDTH - 1,
                             s->data.triangle.x3);
    if (x3 < 0)
      break;
    int y3 = readIntInDialog(dlg, 7, "Vertex 3 Y (0-19)", 0, HEIGHT - 1,
                             s->data.triangle.y3);
    if (y3 < 0)
      break;
    short colorPair = promptColorChoice(dlg, 9, s->color_pair_id);
    if (colorPair < 0)
      break;
    int filled = promptFilledChoice(dlg, 10, s->is_filled);
    if (filled < 0)
      break;
    ok = modifyTriangle(id, x1, y1, x2, y2, x3, y3, colorPair,
                        filled == 1);
    break;
  }
  }

  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return ok;
}

static int confirmDialog(const char *title, const char *message) {
  int dlgH = 7;
  int dlgW = 48;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  int choice = 0;
  const char *options[] = {"Yes", "No"};

  while (1) {
    werase(dlg);
    box(dlg, 0, 0);
    mvwprintw(dlg, 0, 2, " %s ", title);
    mvwprintw(dlg, 2, 2, "%.*s", dlgW - 4, message);
    for (int i = 0; i < 2; i++) {
      if (i == choice) {
        wattron(dlg, A_REVERSE);
        mvwprintw(dlg, 4, 4 + i * 10, " %s ", options[i]);
        wattroff(dlg, A_REVERSE);
      } else {
        mvwprintw(dlg, 4, 4 + i * 10, " %s ", options[i]);
      }
    }
    wrefresh(dlg);

    int ch = wgetch(dlg);
    if (ch == KEY_LEFT || ch == KEY_UP)
      choice = 0;
    else if (ch == KEY_RIGHT || ch == KEY_DOWN)
      choice = 1;
    else if (ch == 27)
      choice = 1;
    else if (ch == '\n' || ch == KEY_ENTER)
      break;
  }

  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return choice == 0;
}

static Shape *findShapeAtPoint(int x, int y) {
  float factor = zoomScale < 0.25f ? 0.25f : zoomScale;

  for (int i = objectCount - 1; i >= 0; i--) {
    Shape *s = &objects[i];

    switch (s->type) {
    case SHAPE_CIRCLE: {
      int cx = scaleCoord(s->data.circle.cx, WIDTH / 2, factor, WIDTH - 1);
      int cy = scaleCoord(s->data.circle.cy, HEIGHT / 2, factor, HEIGHT - 1);
      int r = (int)lrintf((float)s->data.circle.radius * factor);
      if (r < 1)
        r = 1;
      int dx = x - cx;
      int dy = y - cy;
      if (dx * dx + dy * dy <= r * r)
        return s;
      break;
    }
    case SHAPE_RECTANGLE: {
      int x1 = scaleCoord(s->data.rectangle.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.rectangle.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.rectangle.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.rectangle.y2, HEIGHT / 2, factor, HEIGHT - 1);
      int left = (x1 < x2) ? x1 : x2;
      int right = (x1 < x2) ? x2 : x1;
      int top = (y1 < y2) ? y1 : y2;
      int bottom = (y1 < y2) ? y2 : y1;
      if (x >= left && x <= right && y >= top && y <= bottom)
        return s;
      break;
    }
    case SHAPE_LINE: {
      int x1 = scaleCoord(s->data.line.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.line.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.line.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.line.y2, HEIGHT / 2, factor, HEIGHT - 1);
      int dx = x2 - x1;
      int dy = y2 - y1;
      int px = x - x1;
      int py = y - y1;
      int distance = abs(dx * py - dy * px);
      int length = dx * dx + dy * dy;
      if (length > 0 && distance <= 2 && (px * dx + py * dy >= 0) &&
          (px * dx + py * dy <= length))
        return s;
      break;
    }
    case SHAPE_TRIANGLE: {
      int x1 = scaleCoord(s->data.triangle.x1, WIDTH / 2, factor, WIDTH - 1);
      int y1 = scaleCoord(s->data.triangle.y1, HEIGHT / 2, factor, HEIGHT - 1);
      int x2 = scaleCoord(s->data.triangle.x2, WIDTH / 2, factor, WIDTH - 1);
      int y2 = scaleCoord(s->data.triangle.y2, HEIGHT / 2, factor, HEIGHT - 1);
      int x3 = scaleCoord(s->data.triangle.x3, WIDTH / 2, factor, WIDTH - 1);
      int y3 = scaleCoord(s->data.triangle.y3, HEIGHT / 2, factor, HEIGHT - 1);
      if (pointInTriangle(x, y, x1, y1, x2, y2, x3, y3))
        return s;
      break;
    }
    }
  }
  return NULL;
}

static int pickShapeTypeDialog(void) {
  int dlgH = 9;
  int dlgW = 36;
  int startY = (getmaxy(stdscr) - dlgH) / 2;
  int startX = (getmaxx(stdscr) - dlgW) / 2;
  WINDOW *dlg = newwin(dlgH, dlgW, startY, startX);
  keypad(dlg, TRUE);
  int choice = 0;
  const char *options[] = {"Circle", "Rectangle", "Line", "Triangle"};

  while (1) {
    werase(dlg);
    box(dlg, 0, 0);
    mvwprintw(dlg, 0, 2, " Create Shape ");
    mvwprintw(dlg, 2, 2, "Select a shape type:");
    for (int i = 0; i < 4; i++) {
      if (i == choice) {
        wattron(dlg, A_REVERSE);
        mvwprintw(dlg, 4 + i, 4, " %s ", options[i]);
        wattroff(dlg, A_REVERSE);
      } else {
        mvwprintw(dlg, 4 + i, 4, " %s ", options[i]);
      }
    }
    mvwprintw(dlg, 8, 2, "Enter to choose, Esc to cancel");
    wrefresh(dlg);

    int ch = wgetch(dlg);
    if (ch == KEY_UP || ch == KEY_LEFT)
      choice = (choice + 3) % 4;
    else if (ch == KEY_DOWN || ch == KEY_RIGHT)
      choice = (choice + 1) % 4;
    else if (ch == 27)
      return -1;
    else if (ch == '\n' || ch == KEY_ENTER)
      break;
  }

  delwin(dlg);
  touchwin(stdscr);
  refresh();
  return choice;
}

static void handleMenuAction(MenuItem item, char *status, size_t statusLen) {
  switch (item) {
  case MENU_ADD_CIRCLE: {
    int id = dialogAddCircle();
    if (id > 0)
      snprintf(status, statusLen, "Added circle (ID %d)", id);
    else if (id == -1)
      snprintf(status, statusLen, "Add circle cancelled");
    else
      snprintf(status, statusLen, "Error: shape limit reached");
    break;
  }
  case MENU_ADD_RECT: {
    int id = dialogAddRectangle();
    if (id > 0)
      snprintf(status, statusLen, "Added rectangle (ID %d)", id);
    else if (id == -1)
      snprintf(status, statusLen, "Add rectangle cancelled");
    else
      snprintf(status, statusLen, "Error: shape limit reached");
    break;
  }
  case MENU_ADD_LINE: {
    int id = dialogAddLine();
    if (id > 0)
      snprintf(status, statusLen, "Added line (ID %d)", id);
    else if (id == -1)
      snprintf(status, statusLen, "Add line cancelled");
    else
      snprintf(status, statusLen, "Error: shape limit reached");
    break;
  }
  case MENU_ADD_TRIANGLE: {
    int id = dialogAddTriangle();
    if (id > 0)
      snprintf(status, statusLen, "Added triangle (ID %d)", id);
    else if (id == -1)
      snprintf(status, statusLen, "Add triangle cancelled");
    else
      snprintf(status, statusLen, "Error: shape limit reached");
    break;
  }
  case MENU_DELETE: {
    if (objectCount == 0) {
      showMessageDialog("Delete Shape", "No shapes on the canvas.");
      snprintf(status, statusLen, "Nothing to delete");
      break;
    }
    int id = pickObjectDialog("Delete Shape");
    if (id < 0) {
      snprintf(status, statusLen, "Delete cancelled");
      break;
    }
    if (deleteObject(id))
      snprintf(status, statusLen, "Deleted shape ID %d", id);
    else
      snprintf(status, statusLen, "Shape ID %d not found", id);
    break;
  }
  case MENU_MODIFY: {
    if (objectCount == 0) {
      showMessageDialog("Modify Shape", "No shapes on the canvas.");
      snprintf(status, statusLen, "Nothing to modify");
      break;
    }
    int id = pickObjectDialog("Modify Shape");
    if (id < 0) {
      snprintf(status, statusLen, "Modify cancelled");
      break;
    }
    if (dialogModifyShape(id))
      snprintf(status, statusLen, "Modified shape ID %d", id);
    else
      snprintf(status, statusLen, "Modify failed for ID %d", id);
    break;
  }
  case MENU_CLEAR: {
    if (confirmDialog("Clear Canvas", "Remove all shapes from the canvas?")) {
      objectCount = 0;
      nextId = 1;
      redrawCanvas();
      snprintf(status, statusLen, "Canvas cleared");
    } else {
      snprintf(status, statusLen, "Clear cancelled");
    }
    break;
  }
  case MENU_ZOOM_IN: {
    zoomScale *= 1.2f;
    if (zoomScale > 5.0f)
      zoomScale = 5.0f;
    redrawCanvas();
    snprintf(status, statusLen, "Zoom increased to %.2f", zoomScale);
    break;
  }
  case MENU_ZOOM_OUT: {
    zoomScale /= 1.2f;
    if (zoomScale < 0.25f)
      zoomScale = 0.25f;
    redrawCanvas();
    snprintf(status, statusLen, "Zoom decreased to %.2f", zoomScale);
    break;
  }
  case MENU_EXIT:
    break;
  default:
    break;
  }
}

static void initNcurses(void) {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  mouseinterval(0);
  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    init_pair(6, COLOR_GREEN, COLOR_BLACK);
    init_pair(7, COLOR_BLUE, COLOR_BLACK);
    init_pair(8, COLOR_YELLOW, COLOR_BLACK);
    init_pair(9, COLOR_CYAN, COLOR_BLACK);
    init_pair(10, COLOR_MAGENTA, COLOR_BLACK);
  }
  refresh();
}

static void cleanupNcurses(void) {
  if (canvasWin)
    delwin(canvasWin);
  if (menuWin)
    delwin(menuWin);
  if (infoWin)
    delwin(infoWin);
  if (statusWin)
    delwin(statusWin);
  endwin();
}

int main(void) {
  initCanvas();
  initNcurses();

  layoutWindows();

  int menuSelected = 0;
  int running = 1;
  char status[128] =
      "Click canvas or use menu | c=cycle color | +/- zoom | r=resize | q=quit";

  mvprintw(0, (getmaxx(stdscr) - 30) / 2, " 2D GRAPHICS EDITOR WORKSHOP ");
  refresh();

  while (running) {
    refreshAll(menuSelected, status);

    int ch = wgetch(stdscr);
    if (ch == KEY_MOUSE) {
      MEVENT event;
      if (nc_getmouse(&event) == OK && isPrimaryMouseClick(&event)) {
        int inMenu = event.y >= getbegy(menuWin) &&
                     event.y < getbegy(menuWin) + getmaxy(menuWin) &&
                     event.x >= getbegx(menuWin) &&
                     event.x < getbegx(menuWin) + getmaxx(menuWin);

        if (inMenu) {
          int item = event.y - getbegy(menuWin) - 1;
          if (item >= 0 && item < MENU_COUNT) {
            menuSelected = item;
            if (item == MENU_EXIT) {
              running = 0;
            } else {
              handleMenuAction((MenuItem)item, status, sizeof(status));
            }
            continue;
          }
        }

        int inCanvas = event.y >= getbegy(canvasWin) &&
                       event.y < getbegy(canvasWin) + getmaxy(canvasWin) &&
                       event.x >= getbegx(canvasWin) &&
                       event.x < getbegx(canvasWin) + getmaxx(canvasWin);

        if (inCanvas) {
          int px = (event.x - getbegx(canvasWin) - 4) / 2;
          int py = event.y - getbegy(canvasWin) - 2;
          if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            Shape *hit = findShapeAtPoint(px, py);
            if (hit) {
              selectedShapeId = hit->id;
              dialogModifyShape(hit->id);
              redrawCanvas();
              snprintf(status, sizeof(status),
                       "Selected shape ID %d (color: %s)", hit->id,
                       colorNameForPair(hit->color_pair_id));
              continue;
            }

            int type = pickShapeTypeDialog();
            if (type < 0) {
              snprintf(status, sizeof(status), "Create cancelled");
              continue;
            }

            switch (type) {
            case 0:
              addCircle(px, py, 3, activeColorPair, false);
              snprintf(status, sizeof(status),
                       "Added circle at %d,%d (color: %s)", px, py,
                       colorNameForPair(activeColorPair));
              break;
            case 1:
              addRectangle(px, py, px + 4, py + 3, activeColorPair, false);
              snprintf(status, sizeof(status),
                       "Added rectangle at %d,%d (color: %s)", px, py,
                       colorNameForPair(activeColorPair));
              break;
            case 2:
              addLine(px, py, px + 5, py + 2, activeColorPair, false);
              snprintf(status, sizeof(status),
                       "Added line at %d,%d (color: %s)", px, py,
                       colorNameForPair(activeColorPair));
              break;
            case 3:
              addTriangle(px, py, px + 4, py, px + 2, py + 4, activeColorPair,
                          false);
              snprintf(status, sizeof(status),
                       "Added triangle at %d,%d (color: %s)", px, py,
                       colorNameForPair(activeColorPair));
              break;
            }
            continue;
          }
        }
      }
    }

    if (ch == KEY_UP)
      menuSelected = (menuSelected - 1 + MENU_COUNT) % MENU_COUNT;
    else if (ch == KEY_DOWN)
      menuSelected = (menuSelected + 1) % MENU_COUNT;
    else if (ch >= '1' && ch <= '0' + MENU_COUNT)
      menuSelected = ch - '1';
    else if (ch == 'c' || ch == 'C') {
      short previousColor = activeColorPair;
      activeColorPair = nextColorPair(activeColorPair);

      Shape *target = NULL;
      if (selectedShapeId >= 0)
        target = findShapeById(selectedShapeId);
      if (!target && objectCount > 0) {
        target = &objects[objectCount - 1];
        selectedShapeId = target->id;
      }

      if (target) {
        target->color_pair_id = activeColorPair;
        redrawCanvas();
        snprintf(status, sizeof(status),
                 "Color changed to %s. Shape ID %d updated on canvas. Press c again to cycle.",
                 colorNameForPair(activeColorPair), target->id);
      } else {
        redrawCanvas();
        snprintf(status, sizeof(status),
                 "Color changed to %s. No shape selected yet, so the next shape will use it. Press c again to cycle (previous: %s).",
                 colorNameForPair(activeColorPair),
                 colorNameForPair(previousColor));
      }

      continue;
    }
    else if (ch == '+' || ch == '=') {
      zoomScale *= 1.2f;
      if (zoomScale > 5.0f)
        zoomScale = 5.0f;
      redrawCanvas();
      snprintf(status, sizeof(status), "Zoom set to %.2f", zoomScale);
    }
    else if (ch == '-' || ch == '_') {
      zoomScale /= 1.2f;
      if (zoomScale < 0.25f)
        zoomScale = 0.25f;
      redrawCanvas();
      snprintf(status, sizeof(status), "Zoom set to %.2f", zoomScale);
    }
    else if (ch == 'r' || ch == 'R') {
      layoutWindows();
      mvprintw(0, (getmaxx(stdscr) - 30) / 2, " 2D GRAPHICS EDITOR WORKSHOP ");
      clrtoeol();
      refresh();
      redrawCanvas();
      snprintf(status, sizeof(status), "Canvas refreshed after resize");
    }
    else if (ch == 'q' || ch == 'Q')
      running = 0;
    else if (ch == '\n' || ch == KEY_ENTER) {
      if (menuSelected == MENU_EXIT) {
        running = 0;
      } else {
        handleMenuAction((MenuItem)menuSelected, status, sizeof(status));
      }
    } else if (ch == KEY_RESIZE) {
      layoutWindows();
      mvprintw(0, (getmaxx(stdscr) - 30) / 2, " 2D GRAPHICS EDITOR WORKSHOP ");
      clrtoeol();
      refresh();
    }
  }

  cleanupNcurses();
  printf("Goodbye!\n");
  return 0;
}
