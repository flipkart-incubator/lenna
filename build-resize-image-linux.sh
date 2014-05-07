#!/bin/bash
g++ `pkg-config --cflags opencv` `pkg-config --cflags MagickWand` resize-image.cpp -o resize-image `pkg-config --libs opencv` `pkg-config --libs MagickWand`