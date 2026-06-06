#pragma once
#include <stdint.h>

// RGB565 color constants
#define DISPLAY_BLACK   0x0000
#define DISPLAY_WHITE   0xFFFF
#define DISPLAY_RED     0xF800
#define DISPLAY_GREEN   0x07E0
#define DISPLAY_BLUE    0x001F
#define DISPLAY_YELLOW  0xFFE0
#define DISPLAY_CYAN    0x07FF
#define DISPLAY_MAGENTA 0xF81F
#define DISPLAY_ORANGE  0xFD20
#define DISPLAY_PINK    0xFC18

// Forward declaration — TFT_eSPI is an implementation detail, not part of the public API
class TFT_eSPI;

class PicoCalc_Display {
public:
    PicoCalc_Display();

    void begin();
    void clear(uint32_t color = DISPLAY_BLACK);

    void print(int16_t x, int16_t y, const char* text, uint32_t color = DISPLAY_WHITE, uint8_t size = 1);
    void println(int16_t x, int16_t y, const char* text, uint32_t color = DISPLAY_WHITE, uint8_t size = 1);

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
    void drawCircle(int16_t x, int16_t y, int16_t r, uint32_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint32_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color);

    // Bulk pixel streaming — for maximum SPI throughput
    // Usage: startWrite() -> setAddrWindow() -> pushColor() x N -> endWrite()
    void startWrite();                                       // Assert CS, begin SPI transaction
    void endWrite();                                         // Release CS, end SPI transaction
    void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h); // Set pixel write region
    void pushColor(uint16_t color);                          // Send one RGB565 pixel (inside startWrite/endWrite)
    void pushColors(uint16_t* data, uint32_t len);           // Send pixel buffer (inside startWrite/endWrite)

    // Framebuffer — draw to RAM, flush to display in one transfer
    // 320x320 x 2 bytes = 200KB; allocated on heap in begin()
    void      setPixel(int16_t x, int16_t y, uint16_t color); // Write pixel to framebuffer
    uint16_t  getPixel(int16_t x, int16_t y);                 // Read pixel from framebuffer
    void      clearBuffer(uint16_t color = 0x0000);            // Fill entire framebuffer with color
    void      flush();                                          // Send full framebuffer to display (blocking)
    void      flushDMA();                                       // Send full framebuffer via DMA (non-blocking)
    bool      isDMABusy();                                      // Returns true while DMA transfer is in progress
    uint16_t* buf() { return _buf; }                           // Direct framebuffer pointer for fast access

    int16_t width()  { return 320; }
    int16_t height() { return 320; }

    // Access underlying TFT_eSPI instance for advanced use
    TFT_eSPI& tft();

private:
    TFT_eSPI* _tftPtr;  // Heap-allocated to avoid needing full TFT_eSPI definition in header
    uint16_t* _buf = nullptr;  // Heap-allocated framebuffer (200KB)
};
