/**
   RFID_QR.ino

    Created on: 2022-09-20

*/

#include <Arduino.h>
#include <SPI.h>
#include <SPI.h>
#include <EPD1in54.h>
#include <EPDPaint.h>
#include <qrcode.h>
#include <SoftwareSerial.h>

#define COLORED     0
#define UNCOLORED   1

#define RSTPIN 16 // Wemos pin D0
#define RXPIN 12
#define TXPIN 15
#define WS_RST 4
#define WS_DC 0
#define WS_CS 2
#define WS_BUSY 5

unsigned char image[200 * 200 / 8];

EPDPaint paint(image, 200, 200);    // width should be the multiple of 8
EPD1in54 epd(WS_RST, WS_DC, WS_CS, WS_BUSY);

QRCode qrcode;

void drawQrCode(const char* qrStr, char *label) {
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, qrStr);
  paint.clear(UNCOLORED);
  const int pixelsPerDot = 6;
  int margin=(200 - qrcode.size * pixelsPerDot) / 2;
  for (int y = 0; y < qrcode.size; y++) {
      for (int x = 0; x < qrcode.size; x++) {
          if (qrcode_getModule(&qrcode, x, y)) {
              paint.drawFilledRectangle(
                margin + x * pixelsPerDot,
                margin + y * pixelsPerDot,
                margin + x * pixelsPerDot + pixelsPerDot - 1,
                margin + y * pixelsPerDot + pixelsPerDot - 1,
                COLORED);
          }
      }
  }
  //paint.drawStringAt(15, 185, label, &Font16, COLORED);
  epd.setFrameMemory(paint.getImage(), 0, 0, paint.getWidth(), paint.getHeight());
  epd.displayFrame();
}

int hextodec(char digit) {
  if (tolower(digit) >= 'a') {
    return 10 + tolower(digit) - 'a';
  } else {
    return digit - '0';
  }
}

int printable(unsigned char c) {
  return (((c>=32) && (c<127)));
}

void hexdump(sFONT *font, unsigned char *buf, int n, int octetsPerRow, int posx, int posy) {
  static char linebuf[256];
  if (octetsPerRow * 4 + 4 > sizeof(linebuf)) {
    return;
  }
  for (int rowStart = 0; rowStart < n; rowStart += octetsPerRow) {
    memset(linebuf, ' ', sizeof(linebuf));
    linebuf[0] = '\0';
    for (int i=0; i<octetsPerRow; i++) {
      if (i + rowStart < n) {
        sprintf(&linebuf[3*i], "%02x ", buf[i + rowStart]);
        if (printable(buf[i + rowStart])) {
          linebuf[3 * octetsPerRow + 1 + 1 + i] = buf[i + rowStart];
        } else {
          linebuf[3 * octetsPerRow + 1 + 1 + i] = '.';
        }
      } else {
        strcat(linebuf, "   ");
        strcat(&linebuf[3 * octetsPerRow + 1 + 1 + i], " ");
      }
    }
    strcat(linebuf, " ");
    linebuf[3 * octetsPerRow + 1] = '|';
    linebuf[3 * octetsPerRow + 1 + 1 + octetsPerRow] = '|';
    linebuf[3 * octetsPerRow + 1 + 1 + octetsPerRow + 1] = '\0';
    
    paint.drawStringAt(posx, posy + (rowStart / octetsPerRow) * font->Height, linebuf, font, COLORED);
  }
}

void showInfo(unsigned char *buf, int buflen, unsigned long long chipid, unsigned int countrycode) {
  static char strbuf[32], strbuf2[32];
  paint.clear(UNCOLORED);
  hexdump(&Font12, buf, buflen, 6, 5, 5);
  int row = 5 + buflen/6 * Font12.Height;
  if (buf[0] != 2 || buf[29] != 3) {
    paint.drawStringAt(5, row, "First or last byte wrong", &Font12, COLORED);
    row += Font12.Height;
  }
  unsigned char cs=0;
  for (int i=1; i<=26; i++) {
    cs ^= buf[i];
  }
  if (cs == buf[27] && cs == (unsigned char)~buf[28]) {
    paint.drawStringAt(5, row, "Checksum OK", &Font12, COLORED);
    row += Font12.Height;
  } else {
    sprintf(strbuf, "Checksum incorrect (%02x)!", cs);
    paint.drawStringAt(5, row, strbuf, &Font12, COLORED);
    row += Font12.Height;
  }
  for (int i=10; i>0; i--) {
    strbuf2[10-i] = buf[i];
  }
  strbuf2[10] = '\0';
  sprintf(strbuf, "Chipid: %s", strbuf2);
  paint.drawStringAt(5, row, strbuf, &Font12, COLORED);
  row += Font12.Height;

  for (int i=14; i>10; i--) {
    strbuf2[14-i] = buf[i];
  }
  strbuf2[4] = '\0';
  sprintf(strbuf, "Country code: %s", strbuf2);
  paint.drawStringAt(5, row, strbuf, &Font12, COLORED);
  row += Font12.Height;
  
  sprintf(strbuf, "Data flag: %d", buf[15]);
  paint.drawStringAt(5, row, strbuf, &Font12, COLORED);
  row += Font12.Height;
  sprintf(strbuf, "Animal flag: %d", buf[16]);
  paint.drawStringAt(5, row, strbuf, &Font12, COLORED);
  row += Font12.Height;
  sprintf(strbuf, "ID:%03u%012llu", countrycode, chipid);
  paint.drawStringAt(3, row + 20, strbuf, &Font16, COLORED);
  epd.setFrameMemory(paint.getImage(), 0, 0, paint.getWidth(), paint.getHeight());
  epd.displayFrame();

}
SoftwareSerial rfid(RXPIN, TXPIN);

void setup() {
//  pinMode(LED_BUILTIN, OUTPUT);
//  digitalWrite(LED_BUILTIN, LOW);
  if (epd.init(lutFullUpdate) != 0) {
    return;
  }
  epd.clearFrameMemory(0xFF);   // bit set = white, bit reset = black
  epd.displayFrame();
  paint.setRotate(ROTATE_180);
  paint.setWidth(200);
  paint.setHeight(200);
  paint.clear(UNCOLORED);
  paint.drawStringAt(55, 85, "Ready", &Font24, COLORED);
  paint.drawStringAt(40, 107, "to scan!", &Font24, COLORED);
  epd.setFrameMemory(paint.getImage(), 0, 0, paint.getWidth(), paint.getHeight());
  epd.displayFrame();
  pinMode(RSTPIN, OUTPUT);
  Serial.begin(115200);
  rfid.begin(9600); //, SERIAL_8N2);
  digitalWrite(RSTPIN, HIGH);
  delay(200);
  digitalWrite(RSTPIN, LOW);
  delay(200);
  digitalWrite(RSTPIN, HIGH);
    
}

void loop() {
  static int switchMode = (analogRead(A0) > 511);
  static boolean readSomething = false;
  static boolean newCode = false;
  static unsigned char buf[30], tmpbuf[30];
  static char urlbuf[128];
  static unsigned long long chipid = 0;
  static unsigned int countrycode = 0;
  static char strbuf[32];
  int n = rfid.readBytes(buf, 30);
  if (n == 30) {
    readSomething = true;
    newCode = true;
  }
  if (readSomething && (newCode || (analogRead(A0) > 511) != switchMode)) {
    chipid = 0;
    countrycode = 0;
    for (int i=10; i>0; i-=2) {
      chipid <<= 8;
      chipid |= hextodec(buf[i]) << 4 | hextodec(buf[i-1]);
    }
    for (int i=14; i>10; i-=2) {
      countrycode <<= 8;
      countrycode |= hextodec(buf[i]) << 4 | hextodec(buf[i-1]);
    }
    switchMode = (analogRead(A0) > 511);
    newCode = false;
    if (switchMode) {
      showInfo(buf, 30, chipid, countrycode);
    } else {
      sprintf(strbuf, "%03u%012llu", countrycode, chipid);
      sprintf(urlbuf, "https://id.sverak.se/katt.php?idnr=%s", strbuf);
      drawQrCode(urlbuf, strbuf);
    }
  }
}
