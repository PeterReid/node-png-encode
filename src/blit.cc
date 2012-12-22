#include <node.h>
#include <node_buffer.h>
#include <math.h>
#include <algorithm>

#include "macros.h"
#include "blit.h"

using namespace node_png_encode;

static const int BYTES_PER_PIXEL = 4;

static inline int min(int a, int b) {
  return a < b ? a : b;
}
static inline bool between_inex(int low_inclusive, int x, int high_exclusive) {
  return low_inclusive <= x && x < high_exclusive;
}

static uint32_t blend(uint32_t bottom, uint32_t top) {
    // when destAlpha = 0, *destPixel = *sourcePixel;
    // when destAlpha=255, *destPixel

    // it passes through the top layer first. With probability A, the color becomes R, G, B, 1.
    // THis contributes (sR*sA, sG*sA, sB*sA, sA)

    // It passes through the bottom layer. With probability (1-sA)*dA, the color becomes dR, dG, dB, 1
    // THis contributes (x*dR, dG*x, dB*x, x) where x = (1-sA)*dA.

    #define REDOF(x) ((x) & 0xff)
    #define GREENOF(x) (((x)>>8) & 0xff)
    #define BLUEOF(x) (((x)>>16) & 0xff)
    #define ALPHAOF(x) (((x)>>24) & 0xff)
    #define RGBA(r,g,b,a) ((a<<24) | (b<<16) | (g<<8) | (r))

    // Otherwise, it is transparent. Probability 1 - sA - (1-sA)*dA
    int sG = GREENOF(top), sA = ALPHAOF(top);
    if (sA==0) return bottom;
    if (sA==255) return top;

    int dG = GREENOF(bottom), dA = ALPHAOF(bottom);

    int dAAdjusted = ((255-sA)*dA)>>8;

    int outA = sA + dAAdjusted;
    int outG = (sG*sA + dG*dAAdjusted) / outA;
    // These can be combined since the overflow goes into an empty byte
    int outRB = ((top&0x00ff00ff)*sA + ((bottom&0x00ff00ff)*dAAdjusted)) / outA;

    return RGBA(0, outG, 0, outA) | outRB;
}

Handle<Value> node_png_encode::BlitTransparently(const Arguments& args) {
    HandleScope scope;

    REQUIRE_ARGUMENT_BUFFER(0, destBuffer);
    REQUIRE_ARGUMENT_INTEGER(1, destBufferWidth);
    REQUIRE_ARGUMENT_INTEGER(2, destBufferHeight);
    if (destBufferWidth <= 0 || destBufferHeight <= 0 || destBufferWidth*destBufferHeight*BYTES_PER_PIXEL > (int)Buffer::Length(destBuffer)) {
        return ThrowException(Exception::TypeError(
            String::New("Invalid destination width or height"))
        );
    }
    REQUIRE_ARGUMENT_INTEGER(3, destX);
    REQUIRE_ARGUMENT_INTEGER(4, destY);

    REQUIRE_ARGUMENT_BUFFER(5, sourceBuffer);
    REQUIRE_ARGUMENT_INTEGER(6, sourceBufferWidth);
    REQUIRE_ARGUMENT_INTEGER(7, sourceBufferHeight);
    if (sourceBufferWidth <= 0 || sourceBufferHeight <= 0 || sourceBufferWidth*sourceBufferHeight*BYTES_PER_PIXEL > (int)Buffer::Length(sourceBuffer)) {
        return ThrowException(Exception::TypeError(
            String::New("Invalid source width or height"))
        );
    }


    REQUIRE_ARGUMENT_INTEGER(8, sourceX);
    REQUIRE_ARGUMENT_INTEGER(9, sourceY);

    REQUIRE_ARGUMENT_INTEGER(10, copyWidth);
    REQUIRE_ARGUMENT_INTEGER(11, copyHeight);

    uint32_t *destPixels = reinterpret_cast<uint32_t *>(Buffer::Data(destBuffer));
    uint32_t *sourcePixels = reinterpret_cast<uint32_t *>(Buffer::Data(sourceBuffer));

    // TODO: Assumptions to avoid off-of-buffer!

    // Handle source starting in the transparent space above/left of the image
    if (sourceX<0) {
      copyWidth += sourceX;
      destX -= sourceX;
      sourceX = 0;
    }
    if (sourceY<0) {
      copyHeight += sourceY;
      destY -= sourceY;
      sourceY = 0;
    }

    // Handle copying to above and/or left of the dest
    if (destX<0) {
      copyWidth += destX;
      sourceX -= destX;
      destX = 0;
    }
    if (destY<0) {
      copyHeight += destY;
      sourceY -= destY;
      destY = 0;
    }

    // Handle going off the edge
    copyWidth = min(copyWidth, min(destBufferWidth-destX, sourceBufferWidth-sourceX));
    copyHeight = min(copyHeight, min(destBufferHeight-destY, sourceBufferHeight-sourceY));

    if (copyWidth>0 && copyHeight > 0 &&
        between_inex(0,destX,destBufferWidth) &&
        between_inex(0,destY,destBufferHeight) &&
        between_inex(0,sourceX,sourceBufferWidth) &&
        between_inex(0,sourceY,sourceBufferHeight)
    ) {
      uint32_t *destRow = destPixels + destBufferWidth*destY + destX;
      uint32_t *sourceRow = sourcePixels + sourceBufferWidth*sourceY + sourceX;

      for (int row = 0; row < copyHeight; row++, destRow += destBufferWidth, sourceRow += sourceBufferWidth) {
        uint32_t *destPixel = destRow;
        uint32_t *sourcePixel = sourceRow;
        for (int col = 0; col < copyWidth; col++, destPixel++, sourcePixel++) {
          *destPixel = blend(*destPixel, *sourcePixel);
        }
      }
    }
    return scope.Close(Null());
}

void plot(uint32_t *pixels, int w, int h, int x, int y, uint32_t color) {
    if (x<0 || y<0 || x>=w || y>=h) return;

    pixels[w*y + x] = blend(pixels[w*y + x], color);
}

int colorClamp(int x) {
  if (x<0) return 0;
  if (x>255) return 255;
  return x;
}

double ipart(double x) {
  return floor(x);
}
double round(double x) {
  return ipart(x + 0.5);
}
double fpart(double x) {
  return x - ipart(x);
}
double rfpart(double x) {
  return 1 - fpart(x);
}

typedef int32_t fixed_t;

static fixed_t toFixed(double x) {
  return (fixed_t)(x*0x00010000 + .5);
}
static int ipartOfFixed(fixed_t x) {
  return x >> 16;
}


//http://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
Handle<Value> node_png_encode::Line(const Arguments& args) {
    HandleScope scope;

    REQUIRE_ARGUMENT_BUFFER(0, destBuffer);
    REQUIRE_ARGUMENT_INTEGER(1, destBufferWidth);
    REQUIRE_ARGUMENT_INTEGER(2, destBufferHeight);
    if (destBufferWidth <= 0 || destBufferHeight <= 0 || destBufferWidth*destBufferHeight*BYTES_PER_PIXEL > (int)Buffer::Length(destBuffer)) {
        return ThrowException(Exception::TypeError(
            String::New("Invalid destination width or height"))
        );
    }
    uint32_t *destPixels = reinterpret_cast<uint32_t *>(Buffer::Data(destBuffer));

    REQUIRE_ARGUMENT_DOUBLE(3, x0);
    REQUIRE_ARGUMENT_DOUBLE(4, y0);
    REQUIRE_ARGUMENT_DOUBLE(5, x1);
    REQUIRE_ARGUMENT_DOUBLE(6, y1);

    REQUIRE_ARGUMENT_INTEGER(7, red);
    REQUIRE_ARGUMENT_INTEGER(8, green);
    REQUIRE_ARGUMENT_INTEGER(9, blue);

    int BaseColor = RGBA(colorClamp(red), colorClamp(green), colorClamp(blue), 0);

    bool steep = abs(y1 - y0) > abs(x1 - x0);

    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }

    double dx = x1 - x0;
    double dy = y1 - y0;
    if (dx==0) return scope.Close(Undefined());
    double gradient = dy / dx;

    // handle first endpoint
    double xend = round(x0);
    double yend = y0 + gradient * (xend - x0);
    double xgap = rfpart(x0 + 0.5);
    int xpxl1 = (int)xend; // this will be used in the main loop
    int ypxl1 = (int)ipart(yend);
    {
        uint32_t color = ((int)(255*(fpart(yend)*xgap))<<24)  | BaseColor;
        uint32_t colorPrime = ((int)(255*(rfpart(yend)*xgap))<<24) | BaseColor;
        if (steep) {
            plot(destPixels, destBufferWidth, destBufferHeight, ypxl1,   xpxl1,colorPrime);// rfpart(yend) * xgap);
            plot(destPixels, destBufferWidth, destBufferHeight, ypxl1+1, xpxl1, color);// fpart(yend) * xgap);
        } else {
            plot(destPixels, destBufferWidth, destBufferHeight, xpxl1, ypxl1  , colorPrime);//rfpart(yend) * xgap);
            plot(destPixels, destBufferWidth, destBufferHeight, xpxl1, ypxl1+1,  color);//fpart(yend) * xgap);
        }
    }
    double intery = yend + gradient; //first y-intersection for the main loop

    //handle second endpoint
    xend = round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5);
    int xpxl2 = (int)xend;  //this will be used in the main loop
    int ypxl2 = (int)ipart(yend);
    {
        uint32_t color = ((int)(255*(fpart(yend)*xgap))<<24)  | BaseColor;
        uint32_t colorPrime = ((int)(255*(rfpart(yend)*xgap))<<24) | BaseColor;
      if (steep) {
          plot(destPixels, destBufferWidth, destBufferHeight, ypxl2  , xpxl2, colorPrime);//rfpart(yend) * xgap);
          plot(destPixels, destBufferWidth, destBufferHeight, ypxl2+1, xpxl2, color);// fpart(yend) * xgap);
      } else {
          plot(destPixels, destBufferWidth, destBufferHeight, xpxl2, ypxl2,  colorPrime);//rfpart(yend) * xgap);
          plot(destPixels, destBufferWidth, destBufferHeight, xpxl2, ypxl2+1, color);//fpart(yend) * xgap);
      }
    }

    fixed_t fpIntery = toFixed(intery);
    fixed_t fpGradient = toFixed(gradient);

    // Clamp line to left and right edges. I am waiting until we are in int-space to do this
    // so that the result ends up *exactly* the same as if this optimization were not here; no
    // tricky rounding errors. This is useful for testing its correctness.
    if (xpxl2 >= destBufferWidth) xpxl2 = destBufferWidth;
    if (xpxl1 < -1) {
      fpIntery += fpGradient * (-1 - xpxl1);
      xpxl1 = -1;
    }

    // main loop
    for (int x = xpxl1 + 1; x < xpxl2; x++) {
        int alpha = (fpIntery&0xff00)<<16; // Extracting the MSB of the fractional part, shifting to alpha slot
        uint32_t color = alpha | BaseColor;
        uint32_t colorPrime = (0xff000000-alpha) | BaseColor;

        int drawY = ipartOfFixed(fpIntery);
        if (steep) {
            plot(destPixels, destBufferWidth, destBufferHeight, drawY, x, colorPrime);
            plot(destPixels, destBufferWidth, destBufferHeight, drawY+1, x,  color);
        } else {
            plot(destPixels, destBufferWidth, destBufferHeight, x, drawY, colorPrime);
            plot(destPixels, destBufferWidth, destBufferHeight, x, drawY+1,color);
        }

        fpIntery += fpGradient;
    }

   return scope.Close(Null());
}


Handle<Value> node_png_encode::Recolor(const Arguments& args) {
  HandleScope scope;

  REQUIRE_ARGUMENT_BUFFER(0, destBuffer);
  REQUIRE_ARGUMENT_INTEGER(1, red);
  REQUIRE_ARGUMENT_INTEGER(2, green);
  REQUIRE_ARGUMENT_INTEGER(3, blue);

  red = colorClamp(red);
  green = colorClamp(green);
  blue = colorClamp(blue);

  uint32_t color = RGBA(red, green, blue, 0);

  int pixelCount = (int)Buffer::Length(destBuffer) / sizeof(uint32_t);
  uint32_t *destPixels = reinterpret_cast<uint32_t *>(Buffer::Data(destBuffer));
  for (int i = 0; i < pixelCount; i++) {
    destPixels[i] = (0xff000000 & destPixels[i]) | color;
  }

  return scope.Close(Null());
}

Handle<Value> node_png_encode::Batch(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
            String::New("BatchOperations requires operation list and callback")));
  }
  Local<Object> ops = Local<Object>::Cast(args[0]);
  Local<Value> op1 = ops->Get(0);
  //int x = ops->GetLength();
  //
  //printf("Ops length: %d\n", x);
  printf("did it\n");

  return scope.Close(Null());
}