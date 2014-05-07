#!/bin/bash
g++-4.8 `Magick-config --cflags --cppflags` src/main/cpp/resize-image.cpp `Magick-config --ldflags --libs` `pkg-config opencv --libs` /usr/local/Cellar/imagemagick/6.8.8-9/lib/libMagickWand-6.Q16.2.dylib -o resize-image
sudo mv resize-image /usr/bin/resize-image
sudo chmod 755 /usr/bin/resize-image