/***
	Sequential version of the program using standard c++ mechanisms only
***/
#include <dirent.h> 
#include <sstream>
#include <climits>
#include "queue.h"
#include "my_utils.h"

// Watermark
cimg_library::CImg<float> * wmark;


int main(int argc, char* argv[]){

	std::string src_path, wmark_file;
	DIR *dirp;
    struct dirent *directory;
	int sflag = -1, wflag = -1, processed = 0;
	char * end;
	int c;
	float intensity = 0.3;
	const char * USAGE = "Usage -s <src_path> -w <watermark_file> -i <intensity>\n"
	"-s src_path --- Directory containing the images to be watermarked\n"
	"-w watermark_file --- Path of the watermark to be used\n"
	"-i intensity --- Intensity of the watermark image, from 0 (completely transparent) to 100 (completely opaque)\n";

	std::vector<std::thread> workers;

	// Parse command line arguments
	while ((c = getopt (argc, argv, "s:w:i:")) != -1)
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
		std::cerr << "Error loading watermark " << wmark_file << ": " << e.what() << "\nExiting.." << std::endl;	
		exit(1);
	}
	std::cout << "Watermark size: (" << wmark -> width() <<", " << wmark -> height() << ")" << std::endl;

	// Open the directory containing the images to be watermarked
    dirp = opendir(&src_path[0u]);

	long int time_marking = 0;
    if (dirp){
        while ((directory = readdir(dirp)) != NULL){
			if (strendswith(directory->d_name, ".jpg")){

				std::string tmp(directory->d_name);				
				// Open the img and prepare the loading tasks
				std::string load_path = src_path+tmp;
				std::string save_path = src_path+"/watermarked/"+directory->d_name;
				cimg_library::CImg<float> img;
				try{
					img.load(&(load_path)[0u]);
					auto start = std::chrono::high_resolution_clock::now();
				
					int width = wmark -> width();
					int height = wmark -> height();
					int img_height = img.height();
					int img_width = img.width();
					bool has_3_chan = img.spectrum() == 3 && (*wmark).spectrum()==3;
					for (int row = 0; row < img_height; row++)
						for (int col = 0; col < img_width; col++){
							if (!has_3_chan||((*wmark)(col%width,row%height,0,0) + 
								(*wmark)(col%width,row%height,0,1) + 
								(*wmark)(col%width,row%height,0,2) < 500)){
									img(col,row,0,0) = mark_pixel(img(col,row,0,0), (*wmark)(col%width,row%height,0,0), intensity);
									if(has_3_chan){
										img(col,row,0,1) = mark_pixel(img(col,row,0,1), (*wmark)(col%width,row%height,0,1), intensity);		
										img(col,row,0,2) = mark_pixel(img(col,row,0,2), (*wmark)(col%width,row%height,0,2), intensity);	
									}
							}
						}
					auto elapsed = std::chrono::high_resolution_clock::now() - start;
					auto msec    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
					time_marking += msec;
					img.save(&(save_path)[0u]);
					processed += 1;
				}
				catch (const cimg_library::CImgIOException& e) {
				    std::cerr << "Error reading image " << load_path << ": " << e.what() << std::endl;
				}
				catch (const std::exception& e) {
				    std::cerr << "Error saving image " << save_path << ": " << e.what() << std::endl;
				}				

			}
        }

		std::cout << "Spent a total of " << time_marking << " msecs marking images" << std::endl;
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


