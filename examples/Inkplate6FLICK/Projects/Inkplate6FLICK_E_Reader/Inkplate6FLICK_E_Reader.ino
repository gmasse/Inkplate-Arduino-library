/*
    Inkplate6FLICK_E_Reader example for Soldered Inkplate 6FLICK.
    Select "Soldered Inkplate 6FLICK" from the Tools → Board menu.
    Don’t see the "Soldered Inkplate 6FLICK" option? Follow our tutorial and add it:
    https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

    This example uses a preprocessed .epub file and displays it on the screen as images,
    effectively acting as an Open Source eBook reader.

    Required components: Inkplate 6FLICK, SD card, USB-C cable.

    **HOW TO USE THE PYTHON SCRIPT FOR PREPROCESSING**
    Open terminal inside script folder and install dependencies using: pip install -r requirements.txt
    In this example, a Python script named "epubToImg.py" is provided.
    It can be called from the command line with the following arguments:
      [path to epub file] [destination path + book name] --width [number] --height [number] --text-size [size]

    **Some rules to follow:**
      - The book destination must be on the SD card inside the \books\ folder  
        (e.g., D:\books\test-book).
      - When calling the script, use a width of 758 and a height of 930.  
        The UI is designed for these dimensions.
      - If the text size isn’t to your liking, when calling the script, include the OPTIONAL text-size parameter,
        it takes any css compatible size value for example: "14px", "1.5em"... Or you can open the script and directly
        change the DEFAULT_TEXT_SIZE variable. 

    A concrete example for Inkplate 6FLICK with the designed dimensions, with a terminal oppened inside epubToImg folder:
    python .\epubToImg.py "C:\Users\SOLDERED-4\Downloads\pg2000.epub" \
    "D:\books\Don-Quioxte\" --width 758 --height 930 --text-size "14px"

    6 August 2025 by Soldered
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATE6FLICK
#error "Wrong board selection for this example, please select Soldered Inkplate 6FLICK"
#endif

#include "Inkplate.h" //Inkplate display library
#include "SdFat.h"
#include <cstring>   // For strlen, strcmp, strdup
#include <cctype>    // For tolower
#include <cstdlib>   // For malloc, realloc, free, strtol

struct Book { // Saving books in a DLL for easier usage
  char* name;
  Book *next, *previous;
};
struct Picture { // Saving pictures in a DLL for easier usage
  char* name;
  Picture *next, *previous;
};

extern SdFat sd;
Inkplate display(INKPLATE_1BIT);

Book    *headBook = nullptr, *tailBook = nullptr, *currentBook = nullptr;
Picture *headPic  = nullptr, *tailPic  = nullptr, *currentPic  = nullptr;

bool inPictureView = false;
bool inGotoUI      = false;

int totalBooks = 0, totalPics = 0, totalPages = 0;
int firstVisibleBookIndex = 0, visibleRows = 0;
int partialUpdate = 3;

String pageInput = "";

// UI layout
#define MARGIN_X       10
#define MARGIN_Y       30
#define HEADER_MARGIN  50

#define BUTTON_Y  940
#define BUTTON_H   60
#define BUTTON_W  150

// Main view buttons
#define LEFT_BTN_X    ((display.width()/4)   - (BUTTON_W/2))
#define MIDDLE_BTN_X  ((display.width()*3/4) - MARGIN_X - BUTTON_W)
#define RIGHT_BTN_X   ((display.width()*3/4) + MARGIN_X)

// Picture view buttons
#define HOME_BTN_X  ((display.width()*1/8) - (BUTTON_W/2))
#define GOTO_BTN_X  ((display.width()*3/8) - (BUTTON_W/2))
#define PREV_BTN_X  ((display.width()*5/8) - (BUTTON_W/2))
#define NEXT_BTN_X  ((display.width()*7/8) - (BUTTON_W/2))

//Function Prototypes
static bool matchExt(const char *name, const char *ext);
static bool naturalLess(const char *a, const char *b);
void listSubdirectories(const char *path);
void addBook(const char *name);
int  indexOfBook(Book *b);
void listPictures(const char *folderPath);
void addPicture(const char *name);
void freePictures();
int  indexOfPicture(Picture *p);
Picture* getPictureAtIndex(int idx);
void displayMainPage();
void displayButtons();
void displayPicture();
void displayPictureButtons();
static void invertButton(int x, int y, int w, int h, const char* label);
static void displayPageCounter();
void displayGotoUI();

void setup() {
  Serial.begin(115200);
  display.begin();
  display.setRotation(1);
  display.setFullUpdateThreshold(partialUpdate);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.display();

  if (!display.sdCardInit()) {
    Serial.println("SD init failed");
    while (1);
  }
  if (!display.tsInit(true)) {
    Serial.println("Touch init failed");
    while (1);
  }

  // Build book list
  listSubdirectories("/books/");
  currentBook = headBook;

  // figure out how many rows fit
  int lineH = MARGIN_Y + 4;
  visibleRows = (BUTTON_Y - (MARGIN_Y + HEADER_MARGIN)) / lineH;

  if (!headBook) {
    Serial.println("No books found");
    while (1);
  }
  displayMainPage();
}

void loop() {
  if (!inPictureView) {
    // LEFT = SELECT → open pictures
    if (display.touchInArea(LEFT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(LEFT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "SELECT");
      freePictures();
      char folder[128];
      snprintf(folder, sizeof(folder), "/books/%s", currentBook->name);
      listPictures(folder);
      totalPages = totalPics;
      if (headPic) {
        inPictureView = true;
        currentPic = headPic;
        displayPicture();
      } else {
        Serial.println("No images in folder");
      }
      delay(200);
    }
    // PREV book (cyclic if >1)
    else if (display.touchInArea(MIDDLE_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(MIDDLE_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "PREV");
      if (totalBooks > 1) {
        if (currentBook->previous) currentBook = currentBook->previous;
        else                       currentBook = tailBook;
        int idx = indexOfBook(currentBook);
        if (idx < firstVisibleBookIndex)
          firstVisibleBookIndex = idx;
        else if (idx >= firstVisibleBookIndex + visibleRows)
          firstVisibleBookIndex = idx - visibleRows + 1;
        displayMainPage();
      }
      delay(200);
    }
    // NEXT book (cyclic if >1)
    else if (display.touchInArea(RIGHT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(RIGHT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "NEXT");
      if (totalBooks > 1) {
        if (currentBook->next) currentBook = currentBook->next;
        else                   currentBook = headBook;
        int idx = indexOfBook(currentBook);
        if (idx >= firstVisibleBookIndex + visibleRows)
          firstVisibleBookIndex = idx - visibleRows + 1;
        else if (idx < firstVisibleBookIndex)
          firstVisibleBookIndex = idx;
        displayMainPage();
      }
      delay(200);
    }
  } else {
    // GOTO UI keypad handling
    if (inGotoUI) {
      const char* keys[4][3] = {
        {"1","2","3"},
        {"4","5","6"},
        {"7","8","9"},
        {"CLR","0","OK"}
      };
      int spacing = 10, headerH = 40;
      int ovX = MARGIN_X, ovY = MARGIN_Y;
      int ovW = display.width() - 2*MARGIN_X;
      int ovH = BUTTON_Y - 2*MARGIN_Y;
      int totalW = ovW;
      int totalH = ovH - headerH;
      int btnW = (totalW - spacing*2) / 3;
      int btnH = (totalH - spacing*3) / 4;
      int startY = ovY + headerH;
      for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
          int x = ovX + c*(btnW + spacing);
          int y = startY + r*(btnH + spacing);
          if (display.touchInArea(x, y, btnW, btnH)) {
            invertButton(x, y, btnW, btnH, keys[r][c]);
            if (strcmp(keys[r][c], "CLR") == 0) {
              pageInput = "";
              displayGotoUI();
            }
            else if (strcmp(keys[r][c], "OK") == 0) {
              int p = pageInput.toInt();
              if (p < 1)             p = 1;
              else if (p > totalPages) p = totalPages;
              currentPic = getPictureAtIndex(p - 1);
              inGotoUI   = false;
              displayPicture();
            }
            else {
              if (pageInput.length() < 5)
                pageInput += keys[r][c];
              displayGotoUI();
            }
            delay(200);
            return;
          }
        }
      }
    }

    // HOME/BACK button: cancel goto or go home
    if (display.touchInArea(HOME_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      // invert with appropriate label
      if (inGotoUI) invertButton(HOME_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "BACK");
      else          invertButton(HOME_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "HOME");

      if (inGotoUI) {
        inGotoUI = false;
        displayPicture();
      } else {
        freePictures();
        inPictureView = false;
        displayMainPage();
      }
      delay(200);
    }
    // GOTO button → enter numpad
    else if (display.touchInArea(GOTO_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(GOTO_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "GOTO");
      inGotoUI   = true;
      pageInput  = "";  // start empty
      displayGotoUI();
      delay(200);
    }
    // PREV picture
    else if (display.touchInArea(PREV_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(PREV_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "PREV");
      if (!inGotoUI && currentPic->previous) {
        currentPic = currentPic->previous;
        displayPicture();
      }
      delay(200);
    }
    // NEXT picture
    else if (display.touchInArea(NEXT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
      invertButton(NEXT_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, "NEXT");
      if (!inGotoUI && currentPic->next) {
        currentPic = currentPic->next;
        displayPicture();
      }
      delay(200);
    }
  }
}

static bool matchExt(const char *name, const char *ext) {
  size_t n = strlen(name), m = strlen(ext);
  if (n < m) return false;
  const char *p = name + (n - m);
  for (size_t i = 0; i < m; i++)
    if (tolower((unsigned char)p[i]) != tolower((unsigned char)ext[i]))
      return false;
  return true;
}

static bool naturalLess(const char *a, const char *b) {
  char *aend, *bend;
  long anum = strtol(a, &aend, 10);
  long bnum = strtol(b, &bend, 10);
  if (aend != a && bend != b) {
    if (anum != bnum) return anum < bnum;
  }
  return strcmp(a, b) < 0;
}

void listSubdirectories(const char *path) {
  File dir = sd.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry;
  while ((entry = dir.openNextFile())) {
    if (entry.isDirectory()) {
      char name[128];
      entry.getName(name, sizeof(name));
      addBook(name);
    }
    entry.close();
  }
  dir.close();
}

void addBook(const char *name) {
  Book *b = (Book*)malloc(sizeof(Book));
  b->name     = strdup(name);
  b->next     = nullptr;
  b->previous = tailBook;
  if (!headBook) headBook = b;
  else            tailBook->next = b;
  tailBook = b;
  totalBooks++;
}

int indexOfBook(Book *target) {
  int i = 0;
  for (Book *it = headBook; it; it = it->next, i++)
    if (it == target) return i;
  return 0;
}

void listPictures(const char *folderPath) {
  int capacity = 16;
  char **names = (char**)malloc(capacity * sizeof(char*));
  int count = 0;

  File dir = sd.open(folderPath);
  if (!dir || !dir.isDirectory()) return;
  File entry;
  while ((entry = dir.openNextFile())) {
    if (!entry.isDirectory()) {
      char fname[128];
      entry.getName(fname, sizeof(fname));
      if (matchExt(fname, ".bmp") ||
          matchExt(fname, ".jpg") ||
          matchExt(fname, ".jpeg")||
          matchExt(fname, ".png")) {
        if (count >= capacity) {
          capacity *= 2;
          names = (char**)realloc(names, capacity * sizeof(char*));
        }
        names[count++] = strdup(fname);
      }
    }
    entry.close();
  }
  dir.close();

  // sort filenames naturally
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (!naturalLess(names[i], names[j])) {
        char *tmp = names[i];
        names[i] = names[j];
        names[j] = tmp;
      }
    }
  }

  for (int i = 0; i < count; i++) {
    addPicture(names[i]);
    free(names[i]);
  }
  free(names);
}

void addPicture(const char *name) {
  Picture *p = (Picture*)malloc(sizeof(Picture));
  p->name     = strdup(name);
  p->next     = nullptr;
  p->previous = tailPic;
  if (!headPic) headPic = p;
  else           tailPic->next = p;
  tailPic = p;
  totalPics++;
}

void freePictures() {
  for (Picture *p = headPic; p; ) {
    Picture *n = p->next;
    free(p->name);
    free(p);
    p = n;
  }
  headPic    = tailPic = currentPic = nullptr;
  totalPics  = 0;
  totalPages = 0;
}

int indexOfPicture(Picture *target) {
  int i = 0;
  for (Picture *it = headPic; it; it = it->next, i++)
    if (it == target) return i;
  return 0;
}

Picture* getPictureAtIndex(int idx) {
  int i = 0;
  for (Picture *it = headPic; it; it = it->next, i++)
    if (i == idx) return it;
  return headPic;
}

void displayMainPage() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(MARGIN_X, MARGIN_Y);
  display.println("Home - Select a book");
  int y = MARGIN_Y + HEADER_MARGIN;
  int listH = visibleRows * MARGIN_Y;
  display.fillRect(MARGIN_X, y,
                   display.width() - 2*MARGIN_X,
                   listH, WHITE);
  display.setTextSize(2);
  Book *b = headBook;
  for (int i = 0; i < firstVisibleBookIndex && b; i++) b = b->next;
  for (int row = 0; row < visibleRows && b; row++, b = b->next) {
    if (b == currentBook)
      display.drawRect(MARGIN_X, y + row*MARGIN_Y,
                       display.width()-2*MARGIN_X,
                       MARGIN_Y, BLACK);
    display.setCursor(MARGIN_X + 4,
                      y + row*MARGIN_Y + (MARGIN_Y/4));
    display.setTextColor(BLACK);
    display.println(b->name);
  }
  displayButtons();
  display.partialUpdate(false, true);
}

void displayPicture() {
  display.clearDisplay();
  char fullPath[256];
  snprintf(fullPath, sizeof(fullPath),
           "/books/%s/%s",
           currentBook->name, currentPic->name);
  display.drawImage(fullPath, 0, 11, 1);  // moved down by 10px
  displayPictureButtons();
  displayPageCounter();
  display.partialUpdate(false, true);
}

void displayButtons() {
  const char* labels[3] = { "SELECT", "PREV", "NEXT" };
  const int xs[3] = { LEFT_BTN_X, MIDDLE_BTN_X, RIGHT_BTN_X };
  display.setTextSize(2);
  display.setTextColor(BLACK);
  for (int i = 0; i < 3; i++) {
    int x = xs[i], y = BUTTON_Y;
    display.drawRect(x, y, BUTTON_W, BUTTON_H, BLACK);
    int textW = strlen(labels[i]) * 6 * 2;
    int textH = 8 * 2;
    int16_t tx = x + (BUTTON_W - textW) / 2;
    int16_t ty = y + (BUTTON_H - textH) / 2;
    display.setCursor(tx, ty);
    display.print(labels[i]);
  }
}

void displayPictureButtons() {
  const char* labels[4] = { "HOME", "GOTO", "PREV", "NEXT" };
  const int xs[4] = { HOME_BTN_X, GOTO_BTN_X, PREV_BTN_X, NEXT_BTN_X };
  display.setTextSize(2);
  display.setTextColor(BLACK);
  for (int i = 0; i < 4; i++) {
    int x = xs[i], y = BUTTON_Y;
    display.drawRect(x, y, BUTTON_W, BUTTON_H, BLACK);
    int textW = strlen(labels[i]) * 6 * 2;
    int textH = 8 * 2;
    int16_t tx = x + (BUTTON_W - textW) / 2;
    int16_t ty = y + (BUTTON_H - textH) / 2;
    display.setCursor(tx, ty);
    display.print(labels[i]);
  }
}

static void invertButton(int x, int y, int w, int h, const char* label) {
  display.fillRect(x, y, w, h, BLACK);
  display.drawRect(x, y, w, h, WHITE);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  int textW = strlen(label) * 6 * 2;
  int textH = 8 * 2;
  int16_t tx = x + (w - textW) / 2;
  int16_t ty = y + (h - textH) / 2;
  display.setCursor(tx, ty);
  display.print(label);
  display.partialUpdate(false, true);
}

static void displayPageCounter() {
  char buf[32];
  int idx = indexOfPicture(currentPic) + 1;
  snprintf(buf, sizeof(buf), "%d / %d", idx, totalPages);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  int textW = strlen(buf) * 6 * 2;
  int textH = 8 * 2;
  int16_t tx = (display.width() - textW) / 2;
  int16_t ty = BUTTON_Y + BUTTON_H + 5;
  display.setCursor(tx, ty);
  display.print(buf);
}

void displayGotoUI() {
  // hide underlying buttons
  display.fillRect(0, BUTTON_Y, display.width(), BUTTON_H, WHITE);

  // draw BACK button
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.drawRect(HOME_BTN_X, BUTTON_Y, BUTTON_W, BUTTON_H, BLACK);
  const char* backLbl = "BACK";
  int bw = strlen(backLbl) * 6 * 2;
  int bh = 8 * 2;
  int16_t bx = HOME_BTN_X + (BUTTON_W - bw) / 2;
  int16_t by = BUTTON_Y + (BUTTON_H - bh) / 2;
  display.setCursor(bx, by);
  display.print(backLbl);

  // keypad overlay
  int ovX = MARGIN_X, ovY = MARGIN_Y;
  int ovW = display.width() - 2*MARGIN_X;
  int ovH = BUTTON_Y - 2*MARGIN_Y;
  display.fillRect(ovX, ovY, ovW, ovH, WHITE);
  display.drawRect(ovX, ovY, ovW, ovH, BLACK);

  // prompt
  char buf[32];
  snprintf(buf, sizeof(buf), "Go to the page: %s", pageInput.c_str());
  display.setTextSize(2);
  display.setTextColor(BLACK);
  int labelW = strlen(buf) * 6 * 2;
  int16_t lx = (display.width() - labelW) / 2;
  display.setCursor(lx, ovY + 10);
  display.print(buf);

  // keys
  const char* keys[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"CLR","0","OK"}
  };
  int spacing = 10, headerH = 40;
  int totalW = ovW, totalH = ovH - headerH;
  int btnW = (totalW - spacing*2) / 3;
  int btnH = (totalH - spacing*3) / 4;
  int startY = ovY + headerH;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int x = ovX + c*(btnW + spacing);
      int y = startY + r*(btnH + spacing);
      display.drawRect(x, y, btnW, btnH, BLACK);
      int len = strlen(keys[r][c]);
      int textW = len * 6 * 2;
      int textH = 8 * 2;
      int16_t tx = x + (btnW - textW) / 2;
      int16_t ty = y + (btnH - textH) / 2;
      display.setCursor(tx, ty);
      display.print(keys[r][c]);
    }
  }

  display.partialUpdate(false, true);
}