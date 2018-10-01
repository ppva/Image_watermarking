/***
	Parallel version of the program using FastFlow (see @ https://github.com/fastflow/fastflow).
	Loading and saving are not parallelized and put in a pipeline, but rather preloaded in memory.
	Parallelization is done on the marking stage only (the emitter simply sends out the pre-loaded images).
***/

#include <ff/farm.hpp>
#include <ff/pipeline.hpp>
#include <dirent.h> 
#include <sstream>
#include "queue.h"
#include "my_utils.h"

std::atomic<int> processed;

// Watermark
cimg_library::CImg<float> * wmark;

std::vector<task*> tasks;

struct Img_save{
	std::string save_path;
	cimg_library::CImg<float> *img;
};

// Node that emits the image paths (that have been pre-loaded)
struct Emitter : public ff::ff_node_t<task, std::string>{
   Emitter(std::string dir)
        : src_path(dir)
    {
    }
	std::string* svc(task*) {
		for (task *t:tasks)
			ff_send_out(t);
		return EOS;
	}

private:
	std::string src_path;
};


// Node that marks the chunks of the images
struct Marker : public ff::ff_node_t<task, std::string>{
	Marker(float intensity) : _intensity(intensity){};
		std::string *svc(task * t){
			mark_chunk(t -> img, t -> chunk, wmark, _intensity);
			delete(t -> chunk);
			return GO_ON;
		}

private:
	float _intensity;
};


int main(int argc, char* argv[]){

	std::string src_path, wmark_file;
	int sflag = -1, wflag = -1;
	int c, n_workers = 1, n_chunks = 0;
	float intensity = 0.3;
	char * end;
	const char * USAGE = "Usage -s <src_path> -w <watermark_file> -c <chunks> -n <parallelism degree> -i <intensity>\n"
	"-s src_path --- Directory containing the images to be watermarked\n"
	"-w watermark_file --- Path of the watermark to be used\n"
	"-c chunks --- Number of chunks to divide each image in, defaults to parallelism degree\n"
	"-n parallelism degree --- Parallelism degree to be used\n"
	"-i intensity --- Intensity of the watermark image, from 0 (completely transparent) to 100 (completely opaque)\n";
	std::vector<std::thread> workers;
	std::vector<Img_save> images;

	// Parse command line arguments
	while ((c = getopt (argc, argv, "s:w:n:i:c:")) != -1)
		switch (c){
			case 's':
				sflag = 1;
				src_path = optarg;
				mkdir(&(src_path+"watermarked")[0u], 0700);
				break;
			case 'w':
				wflag = 1;
				wmark_file = optarg;
				break;
			case 'n':
				n_workers = strtol(optarg, &end, 10);
				if (*end != '\0') {
					std::cerr << "Invalid number of workers.\n";
					std::cerr << USAGE << std::endl;
					exit(1);
				}
				break;
			case 'c':
				n_chunks = strtol(optarg, &end, 10);
				if (*end != '\0') {
					std::cerr << "Invalid number of chunks.\n";
					std::cerr << USAGE << std::endl;
					exit(1);
				}
				if (n_chunks <= 0){
					std::cerr << "Invalid number of chunks.\n";
					std::cerr << USAGE << std::endl;
					exit(1);	
				}
				break;
			case 'i':
				intensity = strtol(optarg, &end, 10);
				if (*end != '\0') {
					std::cerr << "Invalid intensity.\n";
					std::cerr << USAGE << std::endl;
					exit(1);
				}
				intensity = intensity/100;
				if (intensity < 0 or intensity > 1){
					intensity = 0.3;
					std::cerr << "Intensity set to default " << intensity << " cause given value was out of range (0,100)" << std::endl;
				}
				break;
			case '?':
				if (optopt == 's' || optopt =='w')
					std::cerr << USAGE << std::endl;
				else if (isprint (optopt))
					std::cerr << USAGE << std::endl;
				else
					std::cerr << USAGE << std::endl;
				return 1;
			default:
				exit(1);
		}
	
	// Make sure all the required paths have been set
	if (sflag == -1 || wflag == -1){
		std::cerr << USAGE << std::endl;
		return 1;
	}


	// Load the watermark	
	wmark = new cimg_library::CImg<float>;
	try{
		wmark -> load(&(wmark_file)[0u]);
	}catch (const cimg_library::CImgIOException& e) {
		std::cerr << "Error loading watermark " << wmark_file << ": " << e.what() << "\nExiting.." <<std::endl;	
		exit(1);
	}
	std::cout << "Watermark size: (" << wmark -> width() <<", " << wmark -> height() << ")" << std::endl;

	// If number of chunks has not been specified then set it to the parallelism degree
	if(n_chunks == 0)
		n_chunks = n_workers;

	// Open the directory containing the images to be watermarked
	DIR *dirp;
	struct dirent *directory;
	dirp = opendir(&src_path[0u]);
	if (dirp){
	    while ((directory = readdir(dirp)) != NULL){
			if (strendswith(directory->d_name, ".jpg")){

				cimg_library::CImg<float> * img = new cimg_library::CImg<float>;
				try{
					img -> load(&(src_path +"/" + directory->d_name)[0u]);
				
					
					// Split the image into chunks
					std::vector<img_chunk*> chunks = chunker(img, n_chunks);

					Img_save is;
					is.img = img;
					is.save_path = src_path+"/watermarked/" + directory->d_name;
					images.push_back(is);

					// Push the chunks into a queue
					for (unsigned int i = 0; i < chunks.size(); i++){
						task *t = new task;
						t -> img = img;
						t -> save_path = src_path+"/watermarked/" + directory->d_name;
						t -> chunk = chunks.at(i);
						tasks.push_back(t);
					}
				}catch (const cimg_library::CImgIOException& e) {
					std::cerr << "Error loading image " << (src_path +"/" + directory->d_name) << ": " << e.what() << std::endl;	
				}
			}
	    }
	}
	closedir(dirp);
	delete(directory);


	std::vector<std::unique_ptr<ff::ff_node>> pipes;
	for (int i = 0; i < n_workers; i++) {
		pipes.push_back(ff::make_unique<Marker>(intensity));
	}
	ff::ff_Pipe<task> pipe(
		ff::make_unique<Emitter>(src_path),
		ff::make_unique<ff::ff_Farm<task>>(std::move(pipes))
	);
	auto start = std::chrono::high_resolution_clock::now();
	pipe.run_and_wait_end();
	auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	std::cout << "Elapsed time is " << msec << " msecs " << std::endl;

	// Save the images
	for (Img_save is : images){
		try{
			is.img -> save(&(is.save_path)[0u]);
			processed += 1;
		}
		catch (const std::exception& e) {
            std::cerr << "Error saving image " << is.save_path << ": " << e.what() << std::endl;
        }
		
		delete(is.img);
	}
	for (std::vector<task *>::iterator i = tasks.begin(); i != tasks.end(); ++i) {
		delete *i;
	}
	std::cout << "Processed a total of " << processed << " images" << std::endl;
	delete wmark;
}
