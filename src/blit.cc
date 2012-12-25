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
    int sG = GREENOF(top), sR = REDOF(top), sB = BLUEOF(top), sA = ALPHAOF(top);

    if (sA==0) return bottom;
    if (sA==255) return top;

    int dG = GREENOF(bottom), dR = REDOF(bottom), dB = BLUEOF(bottom), dA = ALPHAOF(bottom);
 

    int dAAdjusted = ((256-sA)*dA)>>8;

    int outA = sA + dAAdjusted;
    int outG = (sG*sA + dG*dAAdjusted) / outA;
	
	int outRBNumerator = ((top&0x00ff00ff)*sA + ((bottom&0x00ff00ff)*dAAdjusted));
	
	int outR = ((outRBNumerator&0x0000ffff) / outA);
	int outB = (((outRBNumerator&0xffff0000) / outA)&0x00ff0000);
	
	return RGBA(outR, outG, 0, outA) | outB;
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
 	pixels[w*y + x] = blend(pixels[w*y + x], color);
}

void boundedPlot(uint32_t *pixels, int w, int h, int x, int y, uint32_t color) {
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

typedef int32_t fixed_t;
const fixed_t FP_HALF = 0x00008000;


static fixed_t fixedRound(fixed_t x) {
	return 0xffff0000&(x + FP_HALF);
}

static fixed_t fixedFloor(fixed_t x) {
	return 0xffff0000&x;
}

static fixed_t fixedCeil(fixed_t x) {
	if ((x&0x0000ffff) == 0) {
		return x;
	}
	return (x&0xffff0000)+0x00010000;
}

static fixed_t fixedMultiply(fixed_t x, fixed_t y) {
	return (int32_t) ((((int64_t) x) * ((int64_t) y)) >>16);
}

static fixed_t fixedDivide(fixed_t numer, fixed_t denom) {
	return (int32_t) ((((int64_t) numer)<<16) / denom);
}

double fpart(double x) {
  return x - ipart(x);
}
double rfpart(double x) {
  return 1 - fpart(x);
}

static fixed_t ffpart(fixed_t x) {
	return x & 0x0000ffff;
}

static fixed_t frfpart(fixed_t x) {
	return 0x00010000 - ffpart(x);
}

static fixed_t frfpartBelowOne(fixed_t x) {
	// 0 < frfpart <= 0x10000, so subtracting one won't wrap to -1
	return frfpart(x) - 1;

}

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

	double dbldx = x1 - x0;
	double dbldy = y1 - y0;
	double dblGradient = dbldx==0 ? 0 : dbldy / dbldx;

	
	
	int yMax = steep ? destBufferWidth - 1 : destBufferHeight - 1;
	int xMax = steep ? destBufferHeight - 1 : destBufferWidth - 1;

	if (y0 < -1) {
		if (dblGradient <= 0) return scope.Close(Null());
		double dbldy = -1 - y0;
		double dbldx = dbldy / dblGradient;
		x0 += dbldx;
		y0 += dbldx * dblGradient;		
	} else if (y0 > yMax) {
		if (dblGradient >= 0) return scope.Close(Null());
		double dbldy =  yMax - y0;
		double dbldx = dbldy / dblGradient;
		x0 += dbldx;
		y0 += dbldx * dblGradient;
	}
	
    // Clamp line to left and right edges. I am waiting until we are in int-space to do this
    // so that the result ends up *exactly* the same as if this optimization were not here; no
    // tricky rounding errors. This is useful for testing its correctness.
	
	if (x0 < -1) {
		y0 += dblGradient * (-1 - x0);
		x0 = -1;
	}
	
	if (x1 > xMax) x1 = xMax;
	//If adjusting x0 to make it start on the image puts x0 out of the image, the line isn't on the image.
	//This also takes care of the double overflowing the fixed point number.
	if (x0 > xMax) return scope.Close(Null());
	if (y0 < -1 || y0 > yMax) return scope.Close(Null());

	fixed_t fpx0 = toFixed(x0);
	fixed_t fpx1 = toFixed(x1);
	fixed_t fpy0 = toFixed(y0);
	fixed_t fpy1 = toFixed(y1);
	fixed_t dx = fpx1 - fpx0;
	fixed_t dy = fpy1 - fpy0;

	fixed_t gradient = dx==0 ? 0 : fixedDivide(dy, dx);

	fixed_t xend = fixedFloor(fpx0);
	
	fixed_t yend = fpy0 + fixedMultiply(gradient, xend - fpx0);

	fixed_t xgap = frfpart(fpx0);// + FP_HALF);
	int xpxl1 = ipartOfFixed(xend);
	int ypxl1 = ipartOfFixed(yend);

	{
		
		uint32_t color = ((ffpart(yend)*xgap) & 0xff000000) | BaseColor;
		// We rely on having at least one factor in the product below less than one
		// and the other less than or equal to one. Otherwise, we end up with a 
		// product that overflows the 32 bits. (0x00010000*0x00010000 = 0).
		// So, we use frfpartBelowOne to get that one strictly less than.
        uint32_t colorPrime = ((frfpartBelowOne(yend)*xgap) & 0xff000000) | BaseColor;
		if (steep) {
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, ypxl1,   xpxl1, colorPrime);// rfpart(yend) * xgap);
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, ypxl1+1, xpxl1, color);// fpart(yend) * xgap);
		} else {
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, xpxl1, ypxl1  , colorPrime);//rfpart(yend) * xgap);
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, xpxl1, ypxl1+1,  color);//fpart(yend) * xgap);
		}
    }
    fixed_t intery = yend + gradient; //first y-intersection for the main loop
    //handle second endpoint
    xend = fixedFloor(fpx1 + 0x10000);
    yend = fpy1 + fixedMultiply(gradient, xend - fpx1);
    xgap = ffpart(fpx1);// + FP_HALF);
    int xpxl2 = ipartOfFixed(xend);  //this will be used in the main loop
    int ypxl2 = ipartOfFixed(yend);

    {
		uint32_t color = ((ffpart(yend)*xgap) & 0xff000000) | BaseColor;
        uint32_t colorPrime = ((frfpartBelowOne(yend)*xgap) & 0xff000000) | BaseColor;

		if (steep) {
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, ypxl2  , xpxl2, colorPrime);//rfpart(yend) * xgap);
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, ypxl2+1, xpxl2, color);// fpart(yend) * xgap);
		} else {
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, xpxl2, ypxl2,  colorPrime);//rfpart(yend) * xgap);
			boundedPlot(destPixels, destBufferWidth, destBufferHeight, xpxl2, ypxl2+1, color);//fpart(yend) * xgap);
		}

    }






	int x = xpxl1 + 1;
	if (gradient >= 0) {
		for (; x < xpxl2; x++) {
			int alpha = (intery&0xff00)<<16; // Extracting the MSB of the fractional part, shifting to alpha slot
			uint32_t color = alpha | BaseColor;

			int drawY = ipartOfFixed(intery);
			
			if (drawY > -1) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY+1, x, color);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY+1, color);
					
			}

			intery += gradient;
		}		
		
		for (; x < xpxl2; x++) {
			int alpha = (intery&0xff00)<<16; // Extracting the MSB of the fractional part, shifting to alpha slot
			uint32_t color = alpha | BaseColor;
			uint32_t colorPrime = (0xff000000-alpha) | BaseColor;

			int drawY = ipartOfFixed(intery);
			
			if (drawY >= yMax) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY, x,  colorPrime);
				plot(destPixels, destBufferWidth, destBufferHeight, drawY+1, x,  color);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY,colorPrime);
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY+1,color);
			}

			intery += gradient;
		}	
		
		for (; x < xpxl2; x++) {
			uint32_t colorPrime = (0xff000000-((intery&0xff00)<<16)) | BaseColor;
			int drawY = ipartOfFixed(intery);
			
			if (drawY > yMax) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY, x,  colorPrime);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY, colorPrime);
			}

			intery += gradient;
		}
	} else if (gradient < 0) {
		for (; x < xpxl2; x++) {
			int alpha = (intery&0xff00)<<16; // Extracting the MSB of the fractional part, shifting to alpha slot
			uint32_t colorPrime = (0xff000000-((intery&0xff00)<<16)) | BaseColor;

			int drawY = ipartOfFixed(intery);
			if (drawY < yMax) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY, x,  colorPrime);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY, colorPrime);
			}

			intery += gradient;
		}		
		
		for (; x < xpxl2; x++) {
			int alpha = (intery&0xff00)<<16; // Extracting the MSB of the fractional part, shifting to alpha slot
			uint32_t color = alpha | BaseColor;
			uint32_t colorPrime = (0xff000000-alpha) | BaseColor;

			int drawY = ipartOfFixed(intery);
			if (drawY <= -1) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY, x,  colorPrime);
				plot(destPixels, destBufferWidth, destBufferHeight, drawY+1, x,  color);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY,colorPrime);
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY+1,color);
			}

			intery += gradient;
		}	
		for (; x < xpxl2; x++) {
			uint32_t color = ((intery&0xff00)<<16) | BaseColor;
			int drawY = ipartOfFixed(intery);
			
			if (drawY < -1) {
				break;
			}
			if (steep) {
				plot(destPixels, destBufferWidth, destBufferHeight, drawY + 1, x,  color);
			} else {
				plot(destPixels, destBufferWidth, destBufferHeight, x, drawY + 1, color);
			}

			intery += gradient;
		}	
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