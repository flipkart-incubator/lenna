#!/bin/bash
g++ `pkg-config --cflags opencv` `pkg-config --cflags MagickWand` resize-image.cpp -o resize-image `pkg-config --libs opencv` `pkg-config --libs MagickWand`
g++ `pkg-config --cflags MagickWand` resize-image-magick.cpp -o resize-image-magick `pkg-config --libs MagickWand`