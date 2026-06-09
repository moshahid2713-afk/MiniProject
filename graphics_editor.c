#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define WIDTH 40
#define HEIGHT 20
#define MAX_OBJECTS 100

// Shape types
typedef enum {
  SHAPE_CIRCLE,
  SHAPE_RECTANGLE,
  SHAPE_LINE,
  SHAPE_TRIANGLE
} ShapeType;

// Base shape structure
typedef struct {
  ShapeType type;
  int id;
  union {
    struct {
      int cx, cy, radius;
    } circle;
    struct {
      int x1, y1, x2, y2;
    } rectangle; // top-left and bottom-right
    struct {
      int x1, y1, x2, y2;
    } line;
    struct {
      int x1, y1, x2, y2, x3, y3;
    } triangle;
  } data;
} Shape;

// Global canvas and object list
char canvas[HEIGHT][WIDTH];
Shape objects[MAX_OBJECTS];
int objectCount = 0;
int nextId = 1;

// Initialize canvas with all spaces (for visual clarity)
void initCanvas() {
  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < WIDTH; j++) {
      canvas[i][j] = ' ';
    }
  }
}

// Display the picture wrapped inside a boxed frame with coordinate indices
void displayPicture() {
  printf("\n");
  // Print top frame border
  printf("   +");
  for (int j = 0; j < WIDTH; j++) {
    printf("--");
  }
  printf("+\n");

  // Print canvas with row indices and vertical borders
  for (int i = 0; i < HEIGHT; i++) {
    printf("%2d |", i);
    for (int j = 0; j < WIDTH; j++) {
      printf("%c ", canvas[i][j]);
    }
    printf("|\n");
  }

  // Print bottom frame border
  printf("   +");
  for (int j = 0; j < WIDTH; j++) {
    printf("--");
  }
  printf("+\n");

  // Print horizontal column coordinate indices
  printf("     ");
  for (int j = 0; j < WIDTH; j += 5) {
    printf("%-10d", j);
  }
  printf("\n\n");
}

// Set a pixel if within bounds
void setPixel(int x, int y) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    canvas[y][x] = '*';
  }
}

// Draw circle using Bresenham's outline approach (hollow)
void drawCirclePixels(int cx, int cy, int radius) {
  if (radius < 0)
    return;
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;
  while (y >= x) {
    setPixel(cx + x, cy + y);
    setPixel(cx - x, cy + y);
    setPixel(cx + x, cy - y);
    setPixel(cx - x, cy - y);
    setPixel(cx + y, cy + x);
    setPixel(cx - y, cy + x);
    setPixel(cx + y, cy - x);
    setPixel(cx - y, cy - x);
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else {
      d = d + 4 * x + 6;
    }
  }
}

// Draw rectangle outline (hollow)
void drawRectanglePixels(int x1, int y1, int x2, int y2) {
  for (int x = x1; x <= x2; x++) {
    setPixel(x, y1);
    setPixel(x, y2);
  }
  for (int y = y1; y <= y2; y++) {
    setPixel(x1, y);
    setPixel(x2, y);
  }
}

// Draw line using Bresenham's algorithm
void drawLinePixels(int x1, int y1, int x2, int y2) {
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    setPixel(x1, y1);
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

// Draw triangle outline (hollow)
void drawTrianglePixels(int x1, int y1, int x2, int y2, int x3, int y3) {
  drawLinePixels(x1, y1, x2, y2);
  drawLinePixels(x2, y2, x3, y3);
  drawLinePixels(x3, y3, x1, y1);
}

// Redraw entire canvas from all objects
void redrawCanvas() {
  initCanvas();
  for (int i = 0; i < objectCount; i++) {
    Shape *s = &objects[i];
    switch (s->type) {
    case SHAPE_CIRCLE:
      drawCirclePixels(s->data.circle.cx, s->data.circle.cy,
                       s->data.circle.radius);
      break;
    case SHAPE_RECTANGLE:
      drawRectanglePixels(s->data.rectangle.x1, s->data.rectangle.y1,
                          s->data.rectangle.x2, s->data.rectangle.y2);
      break;
    case SHAPE_LINE:
      drawLinePixels(s->data.line.x1, s->data.line.y1, s->data.line.x2,
                     s->data.line.y2);
      break;
    case SHAPE_TRIANGLE:
      drawTrianglePixels(s->data.triangle.x1, s->data.triangle.y1,
                         s->data.triangle.x2, s->data.triangle.y2,
                         s->data.triangle.x3, s->data.triangle.y3);
      break;
    }
  }
}

// ADD functions
int addCircle(int cx, int cy, int radius) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_CIRCLE;
  s->id = nextId++;
  s->data.circle.cx = cx;
  s->data.circle.cy = cy;
  s->data.circle.radius = radius;
  redrawCanvas();
  return s->id;
}

int addRectangle(int x1, int y1, int x2, int y2) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_RECTANGLE;
  s->id = nextId++;
  s->data.rectangle.x1 = x1;
  s->data.rectangle.y1 = y1;
  s->data.rectangle.x2 = x2;
  s->data.rectangle.y2 = y2;
  redrawCanvas();
  return s->id;
}

int addLine(int x1, int y1, int x2, int y2) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_LINE;
  s->id = nextId++;
  s->data.line.x1 = x1;
  s->data.line.y1 = y1;
  s->data.line.x2 = x2;
  s->data.line.y2 = y2;
  redrawCanvas();
  return s->id;
}

int addTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
  if (objectCount >= MAX_OBJECTS)
    return -1;
  Shape *s = &objects[objectCount++];
  s->type = SHAPE_TRIANGLE;
  s->id = nextId++;
  s->data.triangle.x1 = x1;
  s->data.triangle.y1 = y1;
  s->data.triangle.x2 = x2;
  s->data.triangle.y2 = y2;
  s->data.triangle.x3 = x3;
  s->data.triangle.y3 = y3;
  redrawCanvas();
  return s->id;
}

// DELETE object by ID
int deleteObject(int id) {
  int found = -1;
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id) {
      found = i;
      break;
    }
  }
  if (found == -1)
    return 0;

  // Shift remaining objects
  for (int i = found; i < objectCount - 1; i++) {
    objects[i] = objects[i + 1];
  }
  objectCount--;
  redrawCanvas();
  return 1;
}

// MODIFY object by ID
int modifyCircle(int id, int cx, int cy, int radius) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_CIRCLE) {
      objects[i].data.circle.cx = cx;
      objects[i].data.circle.cy = cy;
      objects[i].data.circle.radius = radius;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

int modifyRectangle(int id, int x1, int y1, int x2, int y2) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_RECTANGLE) {
      objects[i].data.rectangle.x1 = x1;
      objects[i].data.rectangle.y1 = y1;
      objects[i].data.rectangle.x2 = x2;
      objects[i].data.rectangle.y2 = y2;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

int modifyLine(int id, int x1, int y1, int x2, int y2) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_LINE) {
      objects[i].data.line.x1 = x1;
      objects[i].data.line.y1 = y1;
      objects[i].data.line.x2 = x2;
      objects[i].data.line.y2 = y2;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

int modifyTriangle(int id, int x1, int y1, int x2, int y2, int x3, int y3) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id && objects[i].type == SHAPE_TRIANGLE) {
      objects[i].data.triangle.x1 = x1;
      objects[i].data.triangle.y1 = y1;
      objects[i].data.triangle.x2 = x2;
      objects[i].data.triangle.y2 = y2;
      objects[i].data.triangle.x3 = x3;
      objects[i].data.triangle.y3 = y3;
      redrawCanvas();
      return 1;
    }
  }
  return 0;
}

// List all objects
void listObjects() {
  printf("Objects in picture:\n");
  for (int i = 0; i < objectCount; i++) {
    Shape *s = &objects[i];
    printf("ID %d: ", s->id);
    switch (s->type) {
    case SHAPE_CIRCLE:
      printf("Circle (center=%d,%d, radius=%d)\n", s->data.circle.cx,
             s->data.circle.cy, s->data.circle.radius);
      break;
    case SHAPE_RECTANGLE:
      printf("Rectangle (%d,%d to %d,%d)\n", s->data.rectangle.x1,
             s->data.rectangle.y1, s->data.rectangle.x2, s->data.rectangle.y2);
      break;
    case SHAPE_LINE:
      printf("Line (%d,%d to %d,%d)\n", s->data.line.x1, s->data.line.y1,
             s->data.line.x2, s->data.line.y2);
      break;
    case SHAPE_TRIANGLE:
      printf("Triangle (%d,%d; %d,%d; %d,%d)\n", s->data.triangle.x1,
             s->data.triangle.y1, s->data.triangle.x2, s->data.triangle.y2,
             s->data.triangle.x3, s->data.triangle.y3);
      break;
    }
  }
  if (objectCount == 0)
    printf("  (none)\n");
}

void clearScreen() {
#ifdef _WIN32
  system("cls");
#else
  printf("\033[H\033[J");
  fflush(stdout);
#endif
}

int readInt(const char *prompt, int minVal, int maxVal) {
  int value;
  char line[256];
  while (1) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      return -1; // EOF
    }
    if (sscanf(line, "%d", &value) == 1 && value >= minVal && value <= maxVal) {
      return value;
    }
    printf("  [!] Invalid input. Please enter an integer between %d and %d.\n", minVal, maxVal);
  }
}

int main() {
  initCanvas();

  while (1) {
    clearScreen();
    printf("=========================================================\n");
    printf("*             2D GRAPHICS EDITOR WORKSHOP               *\n");
    printf("=========================================================\n");

    displayPicture();
    listObjects();

    printf("\n--- ACTION MENU ---\n");
    printf("  1. Add a Circle\n");
    printf("  2. Add a Rectangle\n");
    printf("  3. Add a Line\n");
    printf("  4. Add a Triangle\n");
    printf("  5. Delete a Shape by ID\n");
    printf("  6. Clear Canvas\n");
    printf("  7. Exit Workshop\n");
    printf("-------------------\n");

    int choice = readInt("Select an option (1-7): ", 1, 7);
    if (choice == -1 || choice == 7) {
      printf("\nGoodbye!\n");
      break;
    }

    printf("\n");
    if (choice == 1) { // Circle
      printf("--> Drawing a Circle Outline:\n");
      int cx = readInt("  Enter Center X (0 to 39): ", 0, WIDTH - 1);
      int cy = readInt("  Enter Center Y (0 to 19): ", 0, HEIGHT - 1);
      int r = readInt("  Enter Radius (1 to 20): ", 1, 20);

      int id = addCircle(cx, cy, r);
      if (id != -1) {
        printf("\n[+] Successfully added Circle (ID %d)!\n", id);
      } else {
        printf("\n[-] Error: Maximum shapes limit reached!\n");
      }
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);

    } else if (choice == 2) { // Rectangle
      printf("--> Drawing a Rectangle Outline:\n");
      int x1 = readInt("  Enter X1 (Top-Left X, 0 to 39): ", 0, WIDTH - 1);
      int y1 = readInt("  Enter Y1 (Top-Left Y, 0 to 19): ", 0, HEIGHT - 1);
      int x2 = readInt("  Enter X2 (Bottom-Right X, 0 to 39): ", 0, WIDTH - 1);
      int y2 = readInt("  Enter Y2 (Bottom-Right Y, 0 to 19): ", 0, HEIGHT - 1);

      int id = addRectangle(x1, y1, x2, y2);
      if (id != -1) {
        printf("\n[+] Successfully added Rectangle (ID %d)!\n", id);
      } else {
        printf("\n[-] Error: Maximum shapes limit reached!\n");
      }
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);

    } else if (choice == 3) { // Line
      printf("--> Drawing a Line:\n");
      int x1 = readInt("  Enter Start X1 (0 to 39): ", 0, WIDTH - 1);
      int y1 = readInt("  Enter Start Y1 (0 to 19): ", 0, HEIGHT - 1);
      int x2 = readInt("  Enter End X2 (0 to 39): ", 0, WIDTH - 1);
      int y2 = readInt("  Enter End Y2 (0 to 19): ", 0, HEIGHT - 1);

      int id = addLine(x1, y1, x2, y2);
      if (id != -1) {
        printf("\n[+] Successfully added Line (ID %d)!\n", id);
      } else {
        printf("\n[-] Error: Maximum shapes limit reached!\n");
      }
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);

    } else if (choice == 4) { // Triangle
      printf("--> Drawing a Triangle Outline:\n");
      int x1 = readInt("  Enter Vertex 1 X1 (0 to 39): ", 0, WIDTH - 1);
      int y1 = readInt("  Enter Vertex 1 Y1 (0 to 19): ", 0, HEIGHT - 1);
      int x2 = readInt("  Enter Vertex 2 X2 (0 to 39): ", 0, WIDTH - 1);
      int y2 = readInt("  Enter Vertex 2 Y2 (0 to 19): ", 0, HEIGHT - 1);
      int x3 = readInt("  Enter Vertex 3 X3 (0 to 39): ", 0, WIDTH - 1);
      int y3 = readInt("  Enter Vertex 3 Y3 (0 to 19): ", 0, HEIGHT - 1);

      int id = addTriangle(x1, y1, x2, y2, x3, y3);
      if (id != -1) {
        printf("\n[+] Successfully added Triangle (ID %d)!\n", id);
      } else {
        printf("\n[-] Error: Maximum shapes limit reached!\n");
      }
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);

    } else if (choice == 5) { // Delete
      printf("--> Deleting a Shape:\n");
      int id = readInt("  Enter Shape ID to delete: ", 1, 99999);

      if (deleteObject(id)) {
        printf("\n[+] Shape with ID %d successfully deleted!\n", id);
      } else {
        printf("\n[-] Shape with ID %d not found!\n", id);
      }
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);

    } else if (choice == 6) { // Clear
      objectCount = 0;
      nextId = 1;
      redrawCanvas();
      printf("\n[+] Canvas cleared successfully!\n");
      printf("\nPress Enter to continue...");
      char dummy[10];
      fgets(dummy, sizeof(dummy), stdin);
    }
  }

  return 0;
}