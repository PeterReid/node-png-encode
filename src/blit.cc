#include <node.h>
#include <node_buffer.h>


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
      destX = 0;
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
          int sR = REDOF(*sourcePixel), sG = GREENOF(*sourcePixel), sB = BLUEOF(*sourcePixel), sA = ALPHAOF(*sourcePixel);
          if (sA==0) continue;
          
          int dR = REDOF(*destPixel), dG = GREENOF(*destPixel), dB = BLUEOF(*destPixel), dA = ALPHAOF(*destPixel);
          
          int dAAdjusted = ((255-sA)*dA)>>8;
          
          int outA = sA + dAAdjusted;
          int outR = (sR*sA + dR*dAAdjusted) / outA;
          int outG = (sG*sA + dG*dAAdjusted) / outA;
          int outB = (sB*sA + dB*dAAdjusted) / outA;
          
          *destPixel = RGBA(outR, outG, outB, outA);
          
        }
      }
    }
    return scope.Close(Null());
}
