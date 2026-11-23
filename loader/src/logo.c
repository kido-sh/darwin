#include "boot.h"

// the graphical boot screen: black, xnu then draws its bar
void boot_draw_logo(boot_args *args) {
  UINT32 *fb;
  UINT32 width, height, pitch, x, y;

  if (!args || args->Video.v_display != GRAPHICS_MODE || args->Video.v_depth != 32)
    return;

  fb = (UINT32 *)(UINTN)(args->Video.v_baseAddr & ~3ULL);
  width = args->Video.v_width, height = args->Video.v_height;
  pitch = args->Video.v_rowBytes / 4;
  if (!fb || !width || !height)
    return;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      fb[y * pitch + x] = 0;
    }
  }
}
