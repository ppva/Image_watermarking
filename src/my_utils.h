#ifndef __MY_UTILS_H__
#define __MY_UTILS_H__
//#define DEBUG
#include <stdio.h> 
#include <iostream>
#include <ctime>
#include <thread>
#include <functional>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include "CImg.h"
#define EOS nullptr

long int time_it(std::function<void(std::vector<std::thread>*)> f, std::vector<std::thread> * param);

void join_em (std::vector<std::thread> *tids);

bool strendswith(const char* str, const char* suffix);

struct split_path{
	std::string prefix;
	std::string suffix;
};

// Data structure defining a chunk of an image (i.e. start position and end position)
struct img_chunk{
	int s_row;
	int s_col;
	int e_row;
	int e_col;
};

// Data structure defining the tasks (i.e. pointer to image and the chunk to be processed, as well as the number of unprocessed chunks)
struct task {
    cimg_library::CImg<float> * img;
	struct img_chunk * chunk;
	std::atomic<int> * n_chunks;
	std::string save_path;
};

// Function to split an image into n parts - row wise then column wise
std::vector<img_chunk *> chunker(cimg_library::CImg<float> * img, int n);

// Function defining how the image pixel and the watermark one mix up
int mark_pixel(int in_pix, int w_pix, float intensity);

// Function to mark a chunk of image
void mark_chunk(cimg_library::CImg<float> * img,  img_chunk *chunk, cimg_library::CImg<float> *wmark, float intensity);


#endif
