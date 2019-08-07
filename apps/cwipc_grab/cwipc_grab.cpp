#include <iostream>
#include <fstream>
#include "string.h"

#include "cwipc_util/api.h"
#include "cwipc_realsense2/api.h"

int main(int argc, char** argv)
{
    //char *message = NULL;
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " count directory [configfile]" << std::endl;
		std::cerr << "Creates COUNT pointclouds from a realsense2 camera and stores the PLY files in the given DIRECTORY" << std::endl;
		std::cerr << "If directory is - then drop the pointclouds on the floor" << std::endl;
		return 2;
    }
    int count = atoi(argv[1]);
    char filename[500];
    char *error = NULL;
    
	cwipc_tiledsource *generator;
	char *configFile = NULL;
	if (argc == 3) {
		configFile = argv[2];
	}
	generator = cwipc_realsense2(configFile, &error, CWIPC_API_VERSION);
    if (error) {
    	std::cerr << argv[0] << ": creating realsense2 grabber failed: " << error << std::endl;
    	return 1;
    }
	cwipc_tileinfo tif;
	generator->get_tileinfo(0, &tif);
	generator->get_tileinfo(1, &tif);
	generator->get_tileinfo(2, &tif);
	generator->get_tileinfo(3, &tif);
	generator->get_tileinfo(4, &tif);
	int ok = 0;
    while (count-- > 0 && ok == 0) {
    	cwipc *pc = generator->get();
		if (strcmp(argv[2], "-") != 0) {
			snprintf(filename, sizeof(filename), "%s/pointcloud-%lld.ply", argv[2], pc->timestamp());
			ok = cwipc_write(filename, pc, &error);
		}
		pc->free();
    }
    generator->free();
    if (ok < 0) {
    	std::cerr << "Error: " << error << std::endl;
    	return 1;
    }
    return 0;
}

