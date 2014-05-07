#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <wand/magick_wand.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

using namespace std;
using namespace cv;

int convertImage(const char*, const char*);
int resizeImage(const char*, const char*, int, int, bool);

int main(int argc, char *argv[]){
    string src;
    string dest;
    string height;
    string width;
    bool scaled = false;
    bool convert = false;

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
        else if(strcmp(argv[i], "-scaled") == 0){
            scaled = true;
        }
        else if(strcmp(argv[i], "-convert") == 0){
            convert = true;
        }
    }
    
    if(strcmp(src.c_str(), "") == 0 || strcmp(dest.c_str(), "") == 0 || strcmp(width.c_str(), "") == 0 || strcmp(height.c_str(), "") == 0){
        fprintf(stderr, "Invalid Parameters!");
        exit(1);
    }

    if(!(access(src.c_str(), F_OK|R_OK) == 0)){
        fprintf(stderr, "Incorrect permissions on files. Check read write status of provided file names.");
        exit(2);
    }

        printf("Source : %s\nDestination : %s\nWidth : %s\nHeight : %s\nScaled : %c\nConvert : %c\n", src.c_str(), dest.c_str(), width.c_str(), height.c_str(), scaled, convert);
    
    if(convert){
        printf("Converting image to JPG format.\n");
        string temp = string(dest)+"_temp";
        int ret = convertImage(src.c_str(), temp.c_str());
        string source = temp;
        if(ret != 0){
                fprintf(stderr, "Unable to convert image! Proceeding with original source.");
                source = temp;
        }
        ret = resizeImage(source.c_str(), dest.c_str(), atoi(width.c_str()), atoi(height.c_str()), scaled);
        if(strcmp(temp.c_str(), source.c_str()) == 0) {
                printf("Removing %s : %d\n", temp.c_str(), remove(temp.c_str()));
        }
        return ret;
    } else {
        return resizeImage(src.c_str(), dest.c_str(), atoi(width.c_str()), atoi(height.c_str()), scaled);
    }
}

int convertImage(const char* sc, const char* dest){
        MagickWand *m_wand = NULL;
        MagickWandGenesis();
        m_wand = NewMagickWand();
        
        time_t start;
        time_t end;
        // Read the image
        printf("Loading image at %s\n", sc);
        time(&start);
        MagickReadImage(m_wand, sc);
        time(&end);
        printf("Image loaded in %d s.\n", end-start);
        if(m_wand == NULL){
                fprintf(stderr, "Unable to read image at %s\n", sc);
                return -1;
        }
        
        // Set the image format
        time(&start);
        MagickSetImageFormat (m_wand, "JPG");
        time(&end);
        printf("Image processed in %d s.\n", end-start);
        // Write the new image
        time(&start);
        MagickWriteImage(m_wand, dest);
        time(&end);
        printf("Image saved in %d s.\n", end-start);
        
        /* Clean up */
        if(m_wand)m_wand = DestroyMagickWand(m_wand);
        MagickWandTerminus();        
	
	return 0;
}

int resizeImage(const char* sc, const char* dest, int width, int height, bool scale) {
        cout<<"Loading image at "<<sc<<endl;
        IplImage *source = NULL;
        source = cvLoadImage(sc);
        if(source == NULL){
                cout<<"Unable to load image."<<endl;
                return -2;
        }

        cout<<"Image loaded."<<endl;
        
        if(scale){
                printf("Scaling requested.\n");
                int orig_height = source->height;
                int orig_width = source->width;
                if(orig_width >= orig_height) {
                        height = (width < 0)?width:(int) (((double)orig_height/(double)orig_width)*(double)width);
                } else {
                        width = (height < 0)?height:(int) (((double)orig_width/(double)orig_height)*(double)height);
                }
        }
        if(width < 1) width = 1;
        if(height < 1) height = 1;
        printf("Resizing %s to %dx%d\n", sc, width, height);

        IplImage *destination = cvCreateImage( cvSize(width,height), source->depth, source->nChannels );
        cvResize(source, destination, CV_INTER_AREA);
        cvSaveImage( dest, destination);
        
        // clean up
        cvReleaseImage( &source);
        cvReleaseImage( &destination);

        return 0;
}
