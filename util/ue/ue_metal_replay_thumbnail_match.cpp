// SPDX-License-Identifier: MIT
// CPU-only RenderDoc thumbnail resampling and original jpge quality90 encoding.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "renderdoc/3rdparty/jpeg-compressor/jpge.h"

int main(int argc, char **argv)
{
  if(argc != 7) return 2;
  const unsigned width = atoi(argv[2]), height = atoi(argv[3]);
  const unsigned thumbWidth = atoi(argv[4]), thumbHeight = atoi(argv[5]);
  if(!width || !height || !thumbWidth || !thumbHeight || width > 16384 || height > 16384 ||
     thumbWidth > width || thumbHeight != thumbWidth * height / width) return 2;
  std::vector<unsigned char> pixels(size_t(width) * height * 4);
  FILE *input = fopen(argv[1], "rb");
  if(!input) return 3;
  const bool valid = fread(pixels.data(), 1, pixels.size(), input) == pixels.size() &&
                     fgetc(input) == EOF;
  fclose(input);
  if(!valid) return 4;
  std::vector<unsigned char> rgb(size_t(thumbWidth) * thumbHeight * 3);
  for(unsigned y = 0; y < thumbHeight; y++)
    for(unsigned x = 0; x < thumbWidth; x++)
    {
      const auto *src = pixels.data() + 4 * (size_t(y * height / thumbHeight) * width +
                                           x * width / thumbWidth);
      auto *dst = rgb.data() + 3 * (size_t(y) * thumbWidth + x);
      dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0];
    }
  jpge::params parameters; parameters.m_quality = 90;
  return jpge::compress_image_to_jpeg_file(argv[6], thumbWidth, thumbHeight, 3,
                                           rgb.data(), parameters) ? 0 : 5;
}
