#include <Arduino.h>
#include <SMARTGPU.h>
#include <avr/pgmspace.h>

SMARTGPU lcd;

#define DEMO_MODE

#define ICON_NONE 0
#define ICON_HOME 1
#define ICON_BOOK 2
#define ICON_MESG 3
#define ICON_CAMERA 4
#define ICON_PC 5

#define STATUS_DOWN 0
#define STATUS_UP 1

#define MACHINES 3
#define BAR_WIDTH 4
#define POINTS 59  // 238 / BAR_WIDTH
#define NUM_GRAPHS 3
#define GRAPH_HEIGHT 78
#define GRAPH_SEPARATION 20
#define REFRESH_INTERVAL 1000

const char machine_name_0[] PROGMEM = "udoo-1";
const char machine_name_1[] PROGMEM = "udoo-2";
const char machine_name_2[] PROGMEM = "udoo-3";
const char *const machine_names[] PROGMEM = {machine_name_0, machine_name_1, machine_name_2};

const char graph_name_0[] PROGMEM = "CPU";
const char graph_name_1[] PROGMEM = "Requests";
const char graph_name_2[] PROGMEM = "Disk";
const char *const graph_titles[] PROGMEM = {graph_name_0, graph_name_1, graph_name_2};

int graph_data_head = {POINTS - 1};
uint8_t graph_data[NUM_GRAPHS][MACHINES][POINTS] = {0};
int current_graph = -1;
uint8_t machine_status[MACHINES] = {0};

unsigned long long last_refresh = 0;

uint8_t checkIconTouch() {
  char iconbuf[4] = {0};
  if (lcd.touchIcon(iconbuf)) {
    if (strncmp(iconbuf, "HOME", 4) == 0) {
      return ICON_HOME;
    } else if (strncmp(iconbuf, "BOOK", 4) == 0) {
      return ICON_BOOK;
    } else if (strncmp(iconbuf, "MESG", 4) == 0) {
      return ICON_MESG;
    } else if (strncmp(iconbuf, "CAME", 4) == 0) {
      return ICON_CAMERA;
    } else if (strncmp(iconbuf, "PCMO", 4) == 0) {
      return ICON_PC;
    }
  }
  return ICON_NONE;
}

void drawGraphs() {
  for (uint8_t machine = 0; machine < MACHINES; machine++) {
    int top = (GRAPH_SEPARATION * 2) + (machine * GRAPH_HEIGHT + machine * GRAPH_SEPARATION);
    int bottom = top + GRAPH_HEIGHT;

    int j = 1;
    for (int i = graph_data_head; i < POINTS; i++) {
      uint8_t height = ((float)graph_data[current_graph][machine][i] / 100.0) * (GRAPH_HEIGHT / 2);
      if (height) {
        lcd.drawRectangle(j, bottom - 1 - height, j + BAR_WIDTH, bottom - 1, BLUE, FILL);
        lcd.drawRectangle(j, top + 1, j + BAR_WIDTH, bottom - 1 - height, BLACK, FILL);
      }
      j += BAR_WIDTH;
    }
    for (int i = 0; i < graph_data_head; i++) {
      uint8_t height = ((float)graph_data[current_graph][machine][i] / 100.0) * (GRAPH_HEIGHT / 2);
      if (height) {
        lcd.drawRectangle(j, bottom - 1 - height, j + BAR_WIDTH, bottom - 1, BLUE, FILL);
        lcd.drawRectangle(j, top + 1, j + BAR_WIDTH, bottom - 1 - height, BLACK, FILL);
      }
      j += BAR_WIDTH;
    }
  }
}

void showGraph(uint8_t graph) {
  if (current_graph == graph) return;
  current_graph = graph;
  lcd.erase();
  if (current_graph < 0) return;
  char buf[15] = {0};
  strcpy_P(buf, (char *)pgm_read_word(&(graph_titles[current_graph])));
  lcd.string(0, 0, 240, GRAPH_SEPARATION, BLUE, FONT4, FILL, buf);
  for (uint8_t machine = 0; machine < MACHINES; machine++) {
    int top = (GRAPH_SEPARATION * 2) + (machine * GRAPH_HEIGHT + machine * GRAPH_SEPARATION);
    int bottom = top + GRAPH_HEIGHT;
    lcd.drawRectangle(0, top, 239, bottom, RED, UNFILL);

    strcpy_P(buf, (char *)pgm_read_word(&(machine_names[machine])));
    lcd.string(0, top - 15, 240, top - 1, GREEN, FONT2, FILL, buf);
  }
}

void updateMachineStatus(int machine, uint8_t status) {
  machine_status[machine] = status;
  if (current_graph > -1) return;
  int top = 80 + machine * 20;
  int bottom = top + 19;
  lcd.drawRectangle(0, top, 240, bottom, BLACK, FILL);
  char buf[15] = {0};
  strcpy_P(buf, (char *)pgm_read_word(&(machine_names[machine])));
  lcd.string(0, top, 240, bottom, WHITE, FONT3, FILL, buf);
  switch (status) {
    case STATUS_UP:
      lcd.string(120, top, 240, bottom, GREEN, FONT3, FILL, (char *)"UP");
      break;
    case STATUS_DOWN:
      lcd.string(120, top, 240, bottom, RED, FONT3, FILL, (char *)"DOWN");
      break;
  }
}

void drawHome() {
  lcd.erase();

  int x = 75;
  lcd.putLetter(x, 0, BLUE, FONT7, UNFILL, 'G');
  x += 19;
  lcd.putLetter(x, 0, RED, FONT7, UNFILL, 'o');
  x += 18;
  lcd.putLetter(x, 0, YELLOW, FONT7, UNFILL, 'o');
  x += 18;
  lcd.putLetter(x, 0, BLUE, FONT7, UNFILL, 'g');
  x += 14;
  lcd.putLetter(x, 0, GREEN, FONT7, UNFILL, 'l');
  x += 16;
  lcd.putLetter(x, 0, RED, FONT7, UNFILL, 'e');

  lcd.string(15, 30, 240, 320, WHITE, FONT6, UNFILL, (char *)"Micro-Cloud");

  for (int i = 0; i < MACHINES; i++) updateMachineStatus(i, machine_status[i]);
}

void setup() {
  lcd.init();
  lcd.start();
  lcd.baudChange(2000000);
  lcd.orientation(PORTRAITL);
  memset(graph_data, 0, NUM_GRAPHS * MACHINES * POINTS);
  drawHome();
}

int done_ok = 0;

void loop() {
  if (millis() - last_refresh >= REFRESH_INTERVAL) {
    for (uint8_t k = 0; k < NUM_GRAPHS; k++) {
      for (uint8_t machine = 0; machine < MACHINES; machine++) {
#ifdef DEMO_MODE
        graph_data[k][machine][graph_data_head] = random(0, 100);
#endif
      }
    }
    graph_data_head = (graph_data_head + 1) % POINTS;

    if (current_graph > -1) drawGraphs();
    last_refresh = millis();

#ifdef DEMO_MODE
    if (millis() > 5000 && millis() < 7000) updateMachineStatus(1, STATUS_UP);
    if (millis() > 6000 && millis() < 8000) updateMachineStatus(0, STATUS_UP);
    if (millis() > 7000 && millis() < 9000) updateMachineStatus(2, STATUS_UP);
#endif
  }

  if (current_graph < 0) {
    int buf[2] = {0};
    if (lcd.touchScreen(buf)) {
      // Got a touch.
      lcd.putPixel(buf[0], buf[1], BLUE);
    }
  }

  switch (checkIconTouch()) {
    case ICON_NONE:
      break;
    case ICON_HOME:
      if (current_graph > -1) {
        current_graph = -1;
        drawHome();
      }
      break;
    case ICON_BOOK:
      showGraph(0);
      break;
    case ICON_MESG:
      showGraph(1);
      break;
    case ICON_CAMERA:
      showGraph(2);
      break;
    case ICON_PC:
      showGraph(-1);
      break;
  }
}
