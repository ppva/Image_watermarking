/***
	Parallel version of the program using FastFlow (see @ https://github.com/fastflow/fastflow).
	Loading and saving are parallelized as well, thus defining a proper loading-marking-saving pipeline.
***/
#include <ff/farm.hpp>
#include <ff/pipeline.hpp>
#include <dirent.h> 
#include <sstream>
#include <climits>
#include <mutex>
#include "queue.h"
#include "my_utils.h"

std::mutex my_lock;
std::atomic<int> processed;

// Watermark - global as it's only loaded once then workers just read off it
cimg_library::CImg<float> * wmark;

// Node that emits the image paths
struct Emitter : public ff::ff_node_t<task, std::string>{
   Emitter(std::string dir)
        : src_path(dir)
    {
    }
	std::string* svc(task*) {
		// Open the directory containing the images to be watermarked
		DIR *dirp;
    	struct dirent *directory;
		dirp = opendir(&src_path[0u]);
		if (dirp){
		    while ((directory = readdir(dirp)) != NULL){
				if (strendswith(directory->d_name, ".jpg")){
					split_path *sp = new split_path;
					sp -> prefix = src_path+"/";
					sp -> suffix = directory -> d_name;
					ff_send_out(sp);
				}
		    }
		}
		delete(directory);
		closedir(dirp);
		return EOS;
	}

private:
	std::string src_path;
};


// Node that loads the images and splits them into chunks
struct Loader : public ff::ff_node_t<split_path, task>{
	Loader(int chunks) : n_chunks(chunks){}
	
	task *svc(split_path* img_path){
		// Load the image
		cimg_library::CImg<float> * img = new cimg_library::CImg<float>;
		try{
			img -> load(&(img_path->prefix + img_path->suffix)[0u]);
		
			// Split the image into chunks
			std::vector<img_chunk*> chunks = chunker(img, n_chunks);
			std::atomic<int> * num_chunks = new std::atomic<int>;
			*num_chunks = n_chunks;

			// Push the chunks into the next stage
			for (unsigned int i = 0; i < chunks.size(); i++){
				task *t = new task;
				t -> img = img;
				t -> save_path = img_path -> prefix + "watermarked/" + img_path -> suffix;
				t -> chunk = chunks.at(i);
				t -> n_chunks = num_chunks; 
				ff_send_out(t);
			}
		}catch (const cimg_library::CImgIOException& e) {
		    std::cerr << "Error reading image " << img_path->prefix + img_path->suffix << ": " << e.what() << std::endl;
			delete(img);
		}
		delete(img_path);
		return GO_ON;
	}	

private:
	int n_chunks;
};


// Node that marks the chunks of the images
struct Marker : public ff::ff_node_t<task, std::string>{
	Marker(float intensity) : _intensity(intensity){};
		std::string *svc(task * t){
		mark_chunk(t -> img, t -> chunk, wmark, _intensity);
		{
			std::unique_lock<std::mutex> guard(my_lock);
			*t -> n_chunks = *(t -> n_chunks) - 1;

			// Check if its done (all of the chunks have been marked) and hand it over to next stage if that's the case
			if (*(t -> n_chunks) == 0){
				ff_send_out(t);
			}
			else{
				delete(t -> chunk);
				delete(t);
			}
		}
		
		return GO_ON;
	}
private:
	float _intensity;
};


// Node to save the images once all the chunks have been marked
struct Saver : public ff::ff_node_t<task> {
    task* svc(task* t) {
        // Save the image   
        try{
			t -> img -> save(&(t -> save_path)[0u]);
			processed += 1;
		}
		catch (const std::exception& e) {
            std::cerr << "Error saving image " << t -> save_path << ": " << e.what() << std::endl;
        }
        delete(t -> img);
		delete(t -> chunk);
		delete(t -> n_chunks);
		delete(t);
        return GO_ON;

    }
};

int main(int argc, char* argv[]){

	std::string src_path, wmark_file;
	int sflag = -1, wflag = -1;
	int c, n_workers = 1, n_chunks = 0;
	int par_type = 0; // 0 = farm of pipes, 1 = pipe of farms
	float intensity = 0.3;
	char * end;
	const char * USAGE = "Usage -s <src_path> -w <watermark_file> -c <chunks> -n <parallelism degree> -p <parallelism type> -i <intensity>\n"
	"-s src_path --- Directory containing the images to be watermarked\n"
	"-w watermark_file --- Path of the watermark to be used\n"
	"-c chunks --- Number of chunks to divide each image in, defaults to parallelism degree\n"
	"-n parallelism degree --- Parallelism degree to be used\n"
	"-p parallelism type --- 0 = farm of pipes, 1 = pipe of farms\n"
	"-i intensity --- Intensity of the watermark image, from 0 (completely transparent) to 100 (completely opaque)\n";
	std::vector<std::thread> workers;

	// Parse command line arguments
	while ((c = getopt (argc, argv, "s:w:n:p:i:c:")) != -1)
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
			case 'p':
				par_type = strtol(optarg, &end, 10);
				if (*end != '\0' || (par_type!=0 && par_type!=1)) {
					std::cerr << "Invalid parallelism type.\n";
					std::cerr << USAGE << std::endl;
					exit(1);
				}
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
					std::cout << "Intensity set to default " << intensity << " cause given value was out of range (0,100)" << std::endl;
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
		std::cerr << "Error loading watermark " << wmark_file << ": " << e.what() << "\nExiting.." << std::endl;	
	}
	
	std::cout << "Watermark size: (" << wmark -> width() <<", " << wmark -> height() << ")" << std::endl;

	// If number of chunks has not been specified then set it to the parallelism degree
	if(n_chunks == 0)
		n_chunks = n_workers;
	
	std::cout << "Each image will be split in " << n_chunks << " chunks" << std::endl;

 	// Pipeline of farms
	if (par_type == 1){
		std::cout << "PIPE OF FARMS " << std::endl;
		std::vector<std::unique_ptr<ff::ff_node>> loaders;

		for (int i = 0; i < n_workers; i++)
			loaders.push_back(ff::make_unique<Loader>(n_chunks));

		std::vector<std::unique_ptr<ff::ff_node>> markers;
		for (int i = 0; i < n_workers; i++) {
			markers.push_back(ff::make_unique<Marker>(intensity));
		}

		std::vector<std::unique_ptr<ff::ff_node>> savers;
		for (int i = 0; i < n_workers; i++) {
			savers.push_back(ff::make_unique<Saver>());
		}

		ff::ff_Pipe<task> pipe(
			ff::make_unique<Emitter>(src_path),
			ff::make_unique<ff::ff_Farm<task>>(std::move(loaders)),
			ff::make_unique<ff::ff_Farm<task>>(std::move(markers)),
			ff::make_unique<ff::ff_Farm<task>>(std::move(savers))
		);

		auto start = std::chrono::high_resolution_clock::now();
		pipe.run_and_wait_end();
		auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
		std::cout << "Elapsed time is " << msec << " msecs " << std::endl;
	}
	// Farm of pipelines
	else if (par_type == 0){
		std::cout << "FARM OF PIPES " << std::endl;
		std::vector<std::unique_ptr<ff::ff_node>> pipes;
		for (int i = 0; i < n_workers; i++) {
			pipes.push_back(ff::make_unique<ff::ff_Pipe<task>>(
			    ff::make_unique<Loader>(n_workers),
			    ff::make_unique<Marker>(intensity),
			    ff::make_unique<Saver>()
			));
		}
		ff::ff_Pipe<task> pipe(
			ff::make_unique<Emitter>(src_path),
			ff::make_unique<ff::ff_Farm<task>>(std::move(pipes))
		);
		auto start = std::chrono::high_resolution_clock::now();
		pipe.run_and_wait_end();
		auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
		std::cout << "Elapsed time is " << msec << " msecs " << std::endl;
	}
	std::cout << "Processed a total of " << processed << " images" << std::endl;
	delete wmark;
}
