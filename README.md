Setup instructions in the report.

# TL;DR
## Flags:
*`-s src_path` --- Directory containing the images to be watermarked. The processed images will be put in a newly created watermarked directory in the given source directory path.<br/>
*`-w watermark_file` --- Path to the file to be used as watermark.<br/>
*`-c chunks` --- Number of chunks to split each image into. It defaults to the parallelism degree if not specified.<br/>
*`-n parallelism degree` --- Specifies the parallelism degree to run the program with. Defaults to 1 - note that this is not equivalent as the sequential version as setting up a parallel computation presents some overhead.<br/>
*`-p parallelism type` --- Specifies the model to be employed, a value of 0 corresponds to a farm of pipelines and a value of 1 corresponds to a pipeline of farms. Defaults to 0.<br/>
*`-i intensity` --- Specifies the intensity of the watermark image. Ranges from 0 to 100, where 0 corresponds to a completely transparent watermark and 100 to a completely opaque one.

## Setup & run
* `make` 
* Sequential _C++_ version: `out/./seqwatermarker -s "imgs/dataset5/" -w "imgs/watermarks/harambeblack.jpg" -i 30` <br/>
* Parallel standard _C++_ version: `out/./watermarker -s "imgs/dataset5/" -w "imgs/watermarks/harambeblack.jpg" -n 1 -i 30 -c 1` <br/>
* __FastFlow__ version: `out/./ffwatermarker -s "imgs/dataset5/" -w "imgs/watermarks/harambeblack.jpg" -n 1 -i 30 -c 1 -p 0` <br/>
* Restricted __FastFlow__ version: `out/./middleffwatermarker -s "imgs/dataset5/" -w "imgs/watermarks/harambeblack.jpg" -n 1 -i 30 -c 1` <br/>



