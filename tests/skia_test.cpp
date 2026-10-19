#include <include/core/SkSurface.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <cstdio>

int main() {
  printf("Creating SkSurface...\n");
  fflush(stdout);

  SkImageInfo info = SkImageInfo::Make(100, 100, kN32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surf = SkSurfaces::Raster(info);

  if (!surf) {
    printf("FAILED to create surface\n");
    return 1;
  }

  printf("Drawing...\n");
  fflush(stdout);

  SkCanvas* canvas = surf->getCanvas();
  SkPaint p;
  p.setColor(0xFF00FF00);
  canvas->drawRect(SkRect::MakeWH(50, 50), p);

  printf("OK - Skia works!\n");
  return 0;
}