/***
	Parallel version of the program using standard c++ mechanisms only
***/
#include <dirent.h> 
#include <sstream>
#include <climits>
#include "queue.h"
#include "my_utils.h"

std::atomic<int> processed;

// Define the loading tasks (i.e. path of image to load and number of chunks)
struct load_task {
    std::string save_path;
	std::string load_path;
	int n_chunks;
};

// Define the saving tasks (i.e. pointer to image and path to save it in)
struct save_task{
	std::string save_path;
	cimg_library::CImg<float> * img;
};


// Watermark
cimg_library::CImg<float> * wmark;

queue<struct task *> tasks_queue;// Queue for marking tasks
queue<struct load_task *> load_queue; // Queue for loading tasks
queue<struct save_task *> save_queue; // Queue for saving tasks


// Load an image, split into chunks and push into the tasks_queue
void load_and_chunk(std::string path, int n_chunks, std::string save_path){

	// Open the img
	cimg_library::CImg<float> * img = new cimg_library::CImg<float>;
	try{
		img -> load(&(path)[0u]);

		// Split the image into chunks
		std::vector<img_chunk*> chunks = chunker(img, n_chunks);
		struct save_task * st = new save_task;
		st  -> img = img;
		st -> save_path = save_path;
		save_queue.push(st);

		// Push the chunks into a queue
		for (unsigned int i = 0; i < chunks.size(); i++){
			task *t = new task;
			t -> img = img;
			t -> chunk = chunks.at(i);
			t -> save_path = save_path;
			tasks_queue.push(t);

		}
	}catch (const cimg_library::CImgIOException& e) {
	    std::cerr << "Error reading image " << path << ": " << e.what() << std::endl;
		delete(img);
	}
}

// Function to be executed by workers, that parallelizes the loading of the images
void loading_stage(int ti){
	int tn = 0;
    int usecmin = INT_MAX;
    int usecmax = 0;
    long usectot = 0; 

	bool loading = true;
	while (loading){
		auto lt = load_queue.pop();

		if (lt==EOS){
			loading = false;
			#ifdef DEBUG
			std::cout << "Thread loader " << ti << " computed " << tn << " tasks "
			 << " (min max avg (msecs)= " << (usecmin==INT_MAX ? 0:usecmin)/1000 << " " << usecmax/1000
			 << " " << usectot/(tn>0 ? tn:1)/1000 << ") "
			 << std::endl;
			#endif
		}
		else{
			auto start   = std::chrono::high_resolution_clock::now();
			load_and_chunk(lt -> load_path, lt -> n_chunks, lt -> save_path);
			delete(lt);
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
			if(usec < usecmin)
			  usecmin = usec;
			if(usec > usecmax)
			  usecmax = usec;
			usectot += usec;
			tn = tn + 1;
		}
	}
}

// Function to be executed by workers, that parallelizes the saving of the images
void saving_stage(int ti){
	int tn = 0;
    int usecmin = INT_MAX;
    int usecmax = 0;
    long usectot = 0; 

	bool loading = true;
	while (loading){
		auto st = save_queue.pop();

		if (st==EOS){
			loading = false;
			#ifdef DEBUG
			std::cout << "Thread saver " << ti << " computed " << tn << " tasks "
			 << " (min max avg (msecs)= " << (usecmin==INT_MAX ? 0:usecmin)/1000 << " " << usecmax/1000
			 << " " << usectot/(tn>0 ? tn:1)/1000 << ") "
			 << std::endl;
			#endif
		}
		else{
			auto start   = std::chrono::high_resolution_clock::now();
			try{
				st -> img -> save(&(st -> save_path)[0u]);
				processed += 1;
			}
			catch (const std::exception& e) {
            	std::cerr << "Error saving image " << st -> save_path << ": " << e.what() << std::endl;
        	}
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
			delete(st -> img);
			delete(st);
			if(usec < usecmin)
			  usecmin = usec;
			if(usec > usecmax)
			  usecmax = usec;
			usectot += usec;
			tn = tn + 1;
		}
	}
}

// Function to be executed by workers, pops the chunks of images from the queue and processes them
void marking_stage(int ti, float intensity){
	int tn = 0;
    int usecmin = INT_MAX;
    int usecmax = 0;
    long usectot = 0; 

	while (true){
		auto task = tasks_queue.pop();
		if (task==EOS){
			#ifdef DEBUG
			std::cout << "Thread marker " << ti << " computed " << tn << " tasks "
			 << " (min max avg (msecs)= " << (usecmin==INT_MAX ? 0:usecmin)/1000 << " " << usecmax/1000
			 << " " << usectot/(tn>0 ? tn:1)/1000 << ") "
			 << std::endl;
			#endif
			return;
		}
		else{
			auto start   = std::chrono::high_resolution_clock::now();
			// Process the chunk
			mark_chunk(task -> img, task -> chunk, wmark, intensity);	
			/* Don't take into account the time spent saving images  */
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			auto usec    = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
			delete(task -> chunk);
			delete(task);

			if(usec < usecmin)
			  usecmin = usec;
			if(usec > usecmax)
			  usecmax = usec;
			usectot += usec;
			tn = tn + 1;

		}
	}
}


int main(int argc, char* argv[]){
		
	processed = 0;
	std::string src_path, wmark_file;
	DIR *dirp;
    struct dirent *directory;
	int sflag = -1, wflag = -1;
	int c, n_workers = 1, n_chunks = 0, total_chunks = 0;
	float intensity = 0.3;
	char * end;
	const char * USAGE = "Usage -s <src_path> -w <watermark_file> -i <intensity> -c <chunks> -n <parallelism degree>\n"
	"-s src_path --- Directory containing the images to be watermarked\n"
	"-w watermark_file --- Path of the watermark to be used\n"
	"-c chunks --- Number of chunks to divide each image in, defaults to parallelism degree\n"
	"-n parallelism degree --- Parallelism degree to be used\n"
	"-i intensity --- Intensity of the watermark image, from 0 (completely transparent) to 100 (completely opaque)\n";
	std::vector<std::thread> workers;

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
		exit(1);
	}
	std::cout << "Watermark size: (" << wmark -> width() <<", " << wmark -> height() << ")" << std::endl;
	
	// If number of chunks has not been specified then set it to the parallelism degree
	if(n_chunks == 0)
		n_chunks = n_workers;
	
	std::cout << "Each image will be split in " << n_chunks << " chunks" << std::endl;
	// Open the directory containing the images to be watermarked
    dirp = opendir(&src_path[0u]);
	int n_imgs = 0;

    if (dirp){
        while ((directory = readdir(dirp)) != NULL){
			if (strendswith(directory->d_name, ".jpg")){
				std::string tmp(directory->d_name);
				
				// Open the img and prepare the loading tasks
				load_task * lt = new load_task;
				lt -> load_path = src_path+tmp;
				lt -> save_path = src_path+"/watermarked/"+directory->d_name;
				lt -> n_chunks = n_chunks;
				load_queue.push(lt);

				total_chunks = total_chunks + n_chunks;
				n_imgs += 1;
			}
        }
		auto start = std::chrono::high_resolution_clock::now();
		/* LOADING STAGE -- not in a proper pipeline just doing it a tad bit faster for convenience */
		// EOS to signal that no more tasks are available			

		for (int m = 0; m < n_workers; m++){
			load_queue.push(EOS);
		}

		// Initialize the workers
		for (int i=0;i<n_workers;i++)
			workers.push_back(std::thread(loading_stage, i));
        
		for (std::thread& t: workers)
			t.join();

		auto elapsed = std::chrono::high_resolution_clock::now() - start;
		auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		std::cout << "Loading stage done in " << msec << std::endl;
		workers.clear();

		start = std::chrono::high_resolution_clock::now();
		/* MARKING STAGE*/
		// EOS to signal that no more tasks are available			
		for (int m = 0; m < n_workers; m++){
			tasks_queue.push(EOS);
		}

		// Initialize the workers
		for (int i=0;i<n_workers;i++)
			workers.push_back(std::thread(marking_stage, i, intensity));

		for (std::thread& t: workers)
			t.join();

		elapsed = std::chrono::high_resolution_clock::now() - start;
		msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		std::cout << "Processing stage done in " << msec << std::endl;

		workers.clear();

		start = std::chrono::high_resolution_clock::now();
		/* SAVING STAGE -- not in a proper pipeline just doing it a tad bit faster for convenience */
		// EOS to signal that no more tasks are available			
		for (int m = 0; m < n_workers; m++){
			save_queue.push(EOS);
		}

		// Initialize the workers
		for (int i=0;i<n_workers;i++)
			workers.push_back(std::thread(saving_stage, i));
        
		for (std::thread& t: workers)
			t.join();

		elapsed = std::chrono::high_resolution_clock::now() - start;
		msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		std::cout << "Saving stage done in " << msec << std::endl;

		std::cout << "Processed a total of " << processed << " images" << std::endl;
		closedir(dirp);
		delete(wmark);
		delete(directory);
    }
	else{
		std::cerr << "Failed to open directory "<< src_path << std::endl; 
	}
    return(0);
}


