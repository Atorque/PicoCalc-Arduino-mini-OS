#include "PicoCalc_Display.h"
#include <TFT_eSPI.h>

PicoCalc_Display::PicoCalc_Display() {
    _tftPtr = new TFT_eSPI();
}

TFT_eSPI& PicoCalc_Display::tft() { return *_tftPtr; }

void PicoCalc_Display::begin() {
    _tftPtr->init();
    _tftPtr->setRotation(0);
    _tftPtr->invertDisplay(true);
    if (!_buf) _buf = new uint16_t[320 * 320];
    clear(DISPLAY_BLACK);
}

void PicoCalc_Display::clear(uint32_t color) {
    _tftPtr->fillScreen(color);
}

void PicoCalc_Display::print(int16_t x, int16_t y, const char* text, uint32_t color, uint8_t size) {
    _tftPtr->setTextColor(color, DISPLAY_BLACK);
    _tftPtr->setTextSize(size);
    _tftPtr->setCursor(x, y);
    _tftPtr->print(text);
}

void PicoCalc_Display::println(int16_t x, int16_t y, const char* text, uint32_t color, uint8_t size) {
    _tftPtr->setTextColor(color, DISPLAY_BLACK);
    _tftPtr->setTextSize(size);
    _tftPtr->setCursor(x, y);
    _tftPtr->println(text);
}

void PicoCalc_Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    _tftPtr->drawRect(x, y, w, h, color);
}

void PicoCalc_Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    _tftPtr->fillRect(x, y, w, h, color);
}

void PicoCalc_Display::drawCircle(int16_t x, int16_t y, int16_t r, uint32_t color) {
    _tftPtr->drawCircle(x, y, r, color);
}

void PicoCalc_Display::fillCircle(int16_t x, int16_t y, int16_t r, uint32_t color) {
    _tftPtr->fillCircle(x, y, r, color);
}

void PicoCalc_Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color) {
    _tftPtr->drawLine(x0, y0, x1, y1, color);
}

void PicoCalc_Display::startWrite() {
    _tftPtr->startWrite();
}

void PicoCalc_Display::endWrite() {
    _tftPtr->endWrite();
}

void PicoCalc_Display::setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
    _tftPtr->setAddrWindow(x, y, w, h);
}

void PicoCalc_Display::pushColor(uint16_t color) {
    _tftPtr->pushColor(color);
}

void PicoCalc_Display::pushColors(uint16_t* data, uint32_t len) {
    _tftPtr->pushColors(data, len, true);
}

void PicoCalc_Display::setPixel(int16_t x, int16_t y, uint16_t color) {
    if (!_buf || x < 0 || x >= 320 || y < 0 || y >= 320) return;
    _buf[y * 320 + x] = color;
}

uint16_t PicoCalc_Display::getPixel(int16_t x, int16_t y) {
    if (!_buf || x < 0 || x >= 320 || y < 0 || y >= 320) return 0;
    return _buf[y * 320 + x];
}

void PicoCalc_Display::clearBuffer(uint16_t color) {
    if (!_buf) return;
    uint32_t total = 320 * 320;
    for (uint32_t i = 0; i < total; i++) _buf[i] = color;
}

void PicoCalc_Display::flush() {
    if (!_buf) return;
    _tftPtr->startWrite();
    _tftPtr->setAddrWindow(0, 0, 320, 320);
    _tftPtr->pushColors(_buf, 320 * 320, true);
    _tftPtr->endWrite();
}

void PicoCalc_Display::flushDMA() {
    // ILI9488 uses 18-bit SPI colour packing which is incompatible with RP2040 DMA in TFT_eSPI.
    // Fall back to blocking flush.
    flush();
}

bool PicoCalc_Display::isDMABusy() {
    return false;  // No DMA on ILI9488 18-bit SPI
}
