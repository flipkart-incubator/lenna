#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <wand/magick_wand.h>

using namespace std;

int resizeImage(const char*, const char*, int, int, int);

int main(int argc, char *argv[]){
    string src;
    string dest;
    string height;
    string width;
    string quality = "92";

    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "-source") == 0) {
            if(++i < argc){
                src = argv[i];
            }
        }
        else if(strcmp(argv[i], "-dest") == 0){
                if(++i < argc)
                    dest = argv[i];
        }
        else if(strcmp(argv[i], "-w") == 0){
                if(++i < argc)
                    width = argv[i];
        }
        else if(strcmp(argv[i], "-h") == 0){
            if(++i < argc)
                height = argv[i];
        }
        else if(strcmp(argv[i], "-q") == 0){
            if(++i < argc)
                quality = argv[i];
        }
    }
    return resizeImage(src.c_str(), dest.c_str(), atoi(width.c_str()), atoi(height.c_str()), atoi(quality.c_str()));
}

int resizeImage(const char* sc, const char* dest, int width, int height, int quality) {
    cout<<"Loading image at "<<sc<<endl;

    MagickWand *m_wand = NULL;
    MagickWandGenesis();

    m_wand = NewMagickWand();
    MagickReadImage(m_wand, sc);

    int orig_height = MagickGetImageHeight(m_wand);
    int orig_width = MagickGetImageWidth(m_wand);
    if(orig_width >= orig_height) {
            height = (width < 0)?width:(int) (((double)orig_height/(double)orig_width)*(double)width);
    } else {
            width = (height < 0)?height:(int) (((double)orig_width/(double)orig_height)*(double)height);
    }

    MagickResizeImage(m_wand, width, height, LanczosFilter, 1);
    MagickSetImageCompressionQuality(m_wand, quality);
    MagickSetFormat(m_wand, "JPG");
    MagickSetInterlaceScheme(m_wand, PlaneInterlace);
    MagickWriteImage(m_wand, dest);

    ClearMagickWand(m_wand);
    if(m_wand)m_wand = DestroyMagickWand(m_wand);
    MagickWandTerminus();
}