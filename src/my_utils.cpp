//#define DEBUG
#include "my_utils.h"



void join_em (std::vector<std::thread> *tids){
	for (std::thread& t: *tids)
		t.join();
}

long int time_it(std::function<void(std::vector<std::thread>*)> f, std::vector<std::thread> * param){
	auto start = std::chrono::high_resolution_clock::now();
	f(param);
	auto elapsed = std::chrono::high_resolution_clock::now() - start;
	auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

	return msec;
}

bool strendswith(const char* str, const char* suffix)
{
    int len = strlen(str);
    int suffixlen = strlen(suffix);
    if(suffixlen > len)
    {
        return false;
    }

    str += (len - suffixlen);
    return strcmp(str, suffix) == 0;
}

// Function to split an image into n parts - row wise then column wise
std::vector<img_chunk *> chunker(cimg_library::CImg<float> * img, int n){
	std::vector<img_chunk *> chunks;
	int img_width = img -> width();
	int img_height = img -> height();
	int chunk_size = img_width*img_height / n;
	if (chunk_size * n < img_width * img_height) {
		chunk_size+=1;
	}
	#ifdef DEBUG
	std::cout << "Image size: " << img_width*img_height << " -> " << n << " chunks of size: " << chunk_size << std::endl;
	#endif
	int curr_row = 0;
	int curr_col = 0;

	for (int i = 0; i < n; i++){
		img_chunk *chunk = new img_chunk;
		int rows = chunk_size / img_width;
		int cols = chunk_size % img_width;
		cols = cols - 1;
		if (cols == -1){
			rows = rows - 1;
			cols = img_width-1;
		}
		chunk -> s_row = curr_row;
		chunk -> s_col = curr_col;
		bool over = false;
		curr_col = curr_col + cols;
		if (curr_col >= img_width){
			curr_col = curr_col - img_width;
			curr_row = curr_row + 1;
		}
		if ((curr_row + rows) >= img_height){
			curr_row = img_height-1;
			over = true;
		}
		else{ 
			curr_row = curr_row + rows;
		}

		if (over)
			curr_col = img_width - 1;

		chunk -> e_row = curr_row;
		chunk -> e_col = curr_col;
		curr_col = curr_col + 1;
		if (curr_col >= img_width){
			curr_col = 0;
			curr_row = curr_row + 1;		
		}		
		#ifdef DEBUG
		std::cout << "Chunk " << i << " from (" << chunk -> s_row << "," << chunk -> s_col << ") to (" << chunk -> e_row << "," << chunk -> e_col << ")" << std::endl;
		#endif
		chunks.push_back(chunk);	
	}
	return chunks;
}

// Function defining how the image pixel and the watermark one mix up
int mark_pixel(int in_pix, int w_pix, float intensity){
	return in_pix*(1-intensity)+w_pix*intensity;
}

// Function to mark a chunk of image
void mark_chunk(cimg_library::CImg<float> * img,  img_chunk *chunk, cimg_library::CImg<float> *wmark, float intensity){
	int width = wmark -> width();
	int height = wmark -> height();
	int img_width = img -> width() - 1;
	int cond;
	bool has_3_chan = (*img).spectrum() == 3 && (*wmark).spectrum()==3;
	for (int row = chunk -> s_row; row <= chunk -> e_row; row++){
		cond = (row == chunk -> e_row ? chunk -> e_col : img_width);
		for (int col = (row == chunk -> s_row ? chunk -> s_col : 0); 
				col <= cond; col++)
			if (!has_3_chan||((*wmark)(col%width,row%height,0,0) + 
				(*wmark)(col%width,row%height,0,1) + 
				(*wmark)(col%width,row%height,0,2) < 500)){
					(*img)(col,row,0,0) = mark_pixel((*img)(col,row,0,0), (*wmark)(col%width,row%height,0,0), intensity);
					if(has_3_chan){
						(*img)(col,row,0,1) = mark_pixel((*img)(col,row,0,1), (*wmark)(col%width,row%height,0,1), intensity);		
						(*img)(col,row,0,2) = mark_pixel((*img)(col,row,0,2), (*wmark)(col%width,row%height,0,2), intensity);	
					}
			}
	}
}



