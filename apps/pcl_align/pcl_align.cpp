//
//  pcl_align.cpp
//
//  Created by Fons Kuijk on 23-06-18.
//

#include "cwipc_realsense2/window_util.hpp"
#include "cwipc_realsense2/multiFrame.hpp"
#include "cwipc_realsense2/utils.h"

#include <string>
#include <sstream>
#include <pcl/io/ply_io.h>

#define CENTERSTEPS 256

bool align_mode = false;
bool life_align = false;
bool loaded_mode = false;
bool rotation = true;
bool depth_plane = false;
int aligncamera = -1;
cwipc_pcl_pointcloud mergeded_pc;
Eigen::Vector4f mergedcenter;	// Needed to automatically center the merged cloud
Eigen::Vector4f cloudcenter;	// Needed to be able to rotate around the cloud's centre of mass
RS2CaptureConfig configCopy;			// Still copy of the configuration data of multiFrame
std::string ext(".ply");

void printhelp() {
	std::cout << "\nThe cloud rendered by this application will automatically be centered with the view origin.\n";
	std::cout << "To examine the pointcloud use the mouse: leftclick and move to rotate, use the mouse wheel to zoom.\n";
	std::cout << "Use \"esc\" to reset the position of the (fused) cloud.\n";

	std::cout << "\nAction keys for alignment of camera clouds:\n";
    std::cout << "\t\"a\": toggle between 'life' and 'alignment mode'\n";
    std::cout << "\t\"c\": toggle between 'still' and 'continuous'alignment'\n";
	std::cout << "\t\"0-9\": select the camera to manipulate (for align or background, '0' = none\n";
	std::cout << "\t\"r\": start cloud rotate mode\n";
	std::cout << "\t\"t\": start cloud translate mode\n";
	std::cout << "\t\"esc\": reset the cloud transformation of the active camera\n";
	std::cout << "\t\"s\": save the configuration and snapshots of each camera to files\n";
	std::cout << "\t\"l\": toggle between 'life' and a 'loaded' configuration and snapshots to (re)align\n";
    std::cout << "\t\"f\": toggle depth filter\n";
    std::cout << "\t\"b\": toggle background removal\n";
    std::cout << "\t\"arrows\": to move background removal plane\n";
    std::cout << "\t\"z\": return to active background removal\n";
    std::cout << "\t\"d\": toggle visualization depthplane\n";
    std::cout << "\t\"h\": print this help\n";
	std::cout << "\t\"q\": quit program\n";
}

void cloud2file(cwipc_pcl_pointcloud pntcld, std::string filename)
{
	if (!pntcld) return;
	int size = pntcld->size();
	if (size <= 0) return;

	std::ofstream myfile(filename.c_str());
	myfile << "ply\n" << "format ascii 1.0\nelement vertex " << size << "\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";

	std::ostringstream oss;
	for (int i = 0; i < size; i++) {
		oss << (
			std::to_string((*pntcld)[i].x) + " " +
			std::to_string((*pntcld)[i].y) + " " +
			std::to_string((*pntcld)[i].z) + " " +
			std::to_string((*pntcld)[i].r) + " " +
			std::to_string((*pntcld)[i].g) + " " +
			std::to_string((*pntcld)[i].b) + ", " +
			std::to_string((*pntcld)[i].a) + "\n").c_str();
	}
	myfile << oss.str();
	myfile.close();
}

void makeFreezeCopy(RS2Capture* multiframe)
{
	configCopy = multiframe->configuration;
}

// Ignore camera's that may be active, load a configuration and captured frames from file
bool load_data() {

	configCopy.cameraData.clear();

	if (!cwipc_rs2_file2config("cameraconfig.xml", &configCopy))
		return false;

	for (int i = 0; i < configCopy.cameraData.size(); i++) {
		uint64_t ts = 0;
		configCopy.cameraData[i].cloud = cwipc_read((configCopy.cameraData[i].serial + ext).c_str(), ts, NULL, CWIPC_API_VERSION)->access_pcl_pointcloud();
		if (configCopy.cameraData[i].cloud == NULL)
			return false;
	}
	return true;
}

void draw_background_planes(window_util* app, RS2Capture* multiframe) {
	app->prepare_gl(-mergedcenter.x(), -mergedcenter.y(), -mergedcenter.z(), 0);
	for (int i = 0; i < multiframe->configuration.cameraData.size(); i++) {
		RS2Camera* rsd = multiframe->get_camera(multiframe->configuration.cameraData[i].serial);
		cwipc_pcl_pointcloud bgcld(new_cwipc_pcl_pointcloud());

		// generate raster on backgrounf=d
		for (double x = rsd->minx - 2 * rsd->maxz; x < rsd->minx + 2 * rsd->maxz; x += rsd->maxz / 20) {
			for (double y = rsd->minx - 2 * rsd->maxz; y < rsd->minx + 2 * rsd->maxz; y += rsd->maxz / 20) {
				double zx = rsd->minx - x; zx *= zx;
				cwipc_pcl_point pt;
				pt.x = x;
				pt.y = -y;
				pt.z = zx - rsd->maxz;
				pt.r = 0;
				pt.g = 1;
				pt.b = 0;
				bgcld->push_back(pt);
			}
		}
		// generate cameraposition
		double d = 0.005;
		for (double x = -d; x <= d; x += d) {
			for (double y = -d; y <= d; y += d) {
				for (double z = -d; z <= d; z += d) {
					cwipc_pcl_point pt;
					pt.x = x;
					pt.y = y;
					pt.z = z;
					pt.r = 1;
					pt.g = 0;
					pt.b = 0;
					bgcld->push_back(pt);
				}
			}
		}

		cwipc_pcl_pointcloud pcptr(new_cwipc_pcl_pointcloud());
		transformPointCloud(*bgcld, *pcptr, *multiframe->configuration.cameraData[i].trafo);
		for (auto pnt : pcptr->points) {
			float col[3];
			double intens = i == aligncamera ? 0.95 : 0.4;	// highlight the cloud of the selected camera
			col[0] = intens * pnt.r;
			col[1] = intens * pnt.g;
			col[2] = intens * pnt.b;
			glColor3fv(col);
			float vert[] = { pnt.x, pnt.y, pnt.z };
			glVertex3fv(vert);
		}
	}
	app->cleanup_gl();
}

// Handle the OpenGL setup needed to display all pointclouds
void draw_pointclouds(window_util* app, RS2Capture* multiframe)
{
	app->prepare_gl(-mergedcenter.x(), -mergedcenter.y(), -mergedcenter.z(), 0);
	// draw the pointcloud(s)
	if (align_mode) {
        // 'align' mode action: draw the individual pointclouds of the still
		for (int i = 0; i < configCopy.cameraData.size(); i++) {
			cwipc_pcl_pointcloud pcptr(new_cwipc_pcl_pointcloud());
			transformPointCloud(*configCopy.cameraData[i].cloud, *pcptr, *configCopy.cameraData[i].trafo);
			for (auto pnt : pcptr->points) {
				float col[3];
				if (i == aligncamera) {	// highlight the cloud of the selected camera
					col[0] = 0.2 + pnt.r / 320.0;
					col[1] = 0.2 + pnt.g / 320.0;
					col[2] = 0.2 + pnt.b / 320.0;
				}
				else {
					col[0] = pnt.r / 320.0;
					col[1] = pnt.g / 320.0;
					col[2] = pnt.b / 320.0;
				}
				glColor3fv(col);
				float vert[] = { pnt.x, pnt.y, pnt.z };
				glVertex3fv(vert);
			}
		}
	}
	else {
		// 'life' mode action
		if (aligncamera < 0) {
			if (loaded_mode) {
				// this is the stilled 'life' rendering (loaded mode): rendering all individual pointclouds
				for (int i = 0; i < configCopy.cameraData.size(); i++) {
					cwipc_pcl_pointcloud pcptr(new_cwipc_pcl_pointcloud());
					transformPointCloud(*configCopy.cameraData[i].cloud, *pcptr, *configCopy.cameraData[i].trafo);
					for (auto pnt : pcptr->points) {
						float col[] = { (float)pnt.r / 256.f, (float)pnt.g / 256.f, (float)pnt.b / 256.f };
						glColor3fv(col);
						float vert[] = { pnt.x, pnt.y, pnt.z };
						glVertex3fv(vert);
					}
				}
			}
			else if (multiframe->configuration.cameraData.size() > 0) {
				// this is the real 'life' rendering of the merged cloud
				for (auto pnt : multiframe->get_mostRecentPointCloud()->points) {
					float col[] = { (float)pnt.r / 256.f, (float)pnt.g / 256.f, (float)pnt.b / 256.f };
					glColor3fv(col);
					float vert[] = { pnt.x, pnt.y, pnt.z };
					glVertex3fv(vert);
				}
			}
		}
		else {
			// this is the stilled 'life' rendering (loaded mode): rendering one selected pointcloud
			cwipc_pcl_pointcloud pcptr(new_cwipc_pcl_pointcloud());
			transformPointCloud(*configCopy.cameraData[aligncamera].cloud, *pcptr, *configCopy.cameraData[aligncamera].trafo);
			for (auto pnt : pcptr->points) {
				float col[] = { (float)pnt.r / 256.f, (float)pnt.g / 256.f, (float)pnt.b / 256.f };
				glColor3fv(col);
				float vert[] = { pnt.x, pnt.y, pnt.z };
				glVertex3fv(vert);
			}
		}
	}
	app->cleanup_gl();
}

// Registers the state variable and callbacks to allow mouse control of the pointcloud
void register_glfw_callbacks(window_util* app, RS2Capture* multiframe)
{
	app->on_left_mouse = [&](bool pressed) {
		app->app_state()->ml = pressed;
	};

	app->on_mouse_scroll = [&](double xoffset, double yoffset) {
			if (align_mode) {
				Eigen::Affine3d *transform = configCopy.cameraData[aligncamera].trafo.get();
				if (rotation) {
					(*transform).rotate(Eigen::AngleAxisd(yoffset / 100.0, Eigen::Vector3d::UnitZ()));
				}
				else {
					(*transform).translate(Eigen::Vector3d(0.0, 0.0, -yoffset / 100.0));
				}
			}
			else {
				app->app_state()->offset -= static_cast<float>(yoffset);
			}
	};

	app->on_mouse_move = [&](double x, double y) {
		if (app->app_state()->ml) {
				if (align_mode) {
					Eigen::Affine3d *transform = configCopy.cameraData[aligncamera].trafo.get();
					double dx = (x - app->app_state()->last_x) / (0.25 * app->width());
					double dy = -(y - app->app_state()->last_y) / (0.25 * app->width());
					if (rotation) {
						(*transform).translate(Eigen::Vector3d(cloudcenter.x(), cloudcenter.y(), cloudcenter.z()));
						(*transform).rotate(Eigen::AngleAxisd(dx, Eigen::Vector3d::UnitY()));
						(*transform).rotate(Eigen::AngleAxisd(-dy, Eigen::Vector3d::UnitX()));
						(*transform).translate(-Eigen::Vector3d(cloudcenter.x(), cloudcenter.y(), cloudcenter.z()));
					}
					else {
						(*transform).translate(Eigen::Vector3d(dx, dy, 0.0));
					}
				}
				else {
					app->app_state()->yaw += (x - app->app_state()->last_x) / 10.0;
					app->app_state()->pitch += (y - app->app_state()->last_y) / 10.0;
					app->app_state()->pitch = std::max(app->app_state()->pitch, -85.0);
					app->app_state()->pitch = std::min(app->app_state()->pitch, +85.0);
				}
		}
		app->app_state()->last_x = x;
		app->app_state()->last_y = y;
	};

	app->on_key_release = [&](int key) {
		if (key == 256) { // Escape is interpreted as a reset of the transformation
				if (align_mode) {
					Eigen::Affine3d *transform = configCopy.cameraData[aligncamera].trafo.get();
					(*transform).setIdentity();
				}
				else {
					app->app_state()->yaw = app->app_state()->pitch = 0;
					app->app_state()->offset = 0.0;
				}
		}
		else if (key == 65) {	// key = "a": start/stop Alignment
			if (align_mode) {
                // switch to 'life'
                if (!loaded_mode && multiframe->configuration.cameraData.size() > 0)
					multiframe->configuration = configCopy; // set possible new transforms
				align_mode = false;
				aligncamera = -1;
			}
			else {
                // switch to 'align'
                if (!loaded_mode && multiframe->configuration.cameraData.size() > 0)
                    makeFreezeCopy(multiframe); // make a still copy of multiFrame's configuration
				if (aligncamera < 0 || aligncamera >= configCopy.cameraData.size())
					aligncamera = 0;
				pcl::compute3DCentroid(*configCopy.cameraData[aligncamera].cloud, cloudcenter);
				align_mode = true;
			}
		}
		else if (key == 67) {	// key = "c": toggle still or life alignment
			if (life_align)
				life_align = false;
			else
				life_align = true;
		}
        else if (key == 68) {    // key = "d": toggle show depth plane
            if (depth_plane)
				depth_plane = false;
            else
				depth_plane = true;
        }
		else if (key == 72) {	// key = "h": print help
			printhelp();
		}
		else if (key == 76) {	// key = "l": load previous result
			if (loaded_mode) {
				// leaving loaded mode
                if (multiframe->configuration.cameraData.size() > 0) {
                    makeFreezeCopy(multiframe); // make a still copy of multiFrame's configuration
					align_mode = false; // that is the to be expected mode
					aligncamera = -1;
				}
				loaded_mode = false;
				std::cout << "loaded mode switched off\n";
			}
			else {
				// starting loaded mode
				if (load_data() && configCopy.cameraData.size() > 0)
					align_mode = true; // that is the to be expected mode
				else {
					std::cerr << "\npcl_align: Error: Data could not be loaded\n";
					makeFreezeCopy(multiframe); // make a still copy of multiFrame's configuration
					return;
				}
				if (aligncamera < 0 || aligncamera >= configCopy.cameraData.size())
					aligncamera = 0;
				pcl::compute3DCentroid(*configCopy.cameraData[aligncamera].cloud, cloudcenter);
				loaded_mode = true;
				std::cout << "loaded mode switched on\n";
			}
		}
		else if (key == 81) {	// key = "q": Quit program
            multiframe->~RS2Capture();
			exit(0);
		}
		else if (key == 82) {	// key = "r": Rotate
			rotation = true;
		}
		else if (key == 83) {	// key = "s": Save config and snapshots to file
			// saving transformations
			cwipc_rs2_config2file("cameraconfig.xml", &configCopy);

			if (!loaded_mode) {	// save the clouds themselves
				if (align_mode)
					// saving the stilled capture
					for (auto cd : configCopy.cameraData)
						cloud2file(cd.cloud, cd.serial + ext);
				else
					// saving snapshot of life capturing
					for (auto cd : multiframe->configuration.cameraData)
						cloud2file(cd.cloud, cd.serial + ext);
				cloud2file(mergeded_pc, "merged_pc" + ext);
			}
		}
		else if (key == 84) {	// key = "t": Translate
			rotation = false;
		}
		else if (key >= 48 && key < configCopy.cameraData.size() + 49) {	// key = "0-9": select a camera ("0" = none)
			aligncamera = key - 49;
			if (aligncamera >= 0)
			pcl::compute3DCentroid(*configCopy.cameraData[aligncamera].cloud, cloudcenter);
		}
		else if (key == 73) {	// key =\"i": dump frames for icp processing
			for (int i = 0; i < configCopy.cameraData.size(); i++) {
				cwipc_pcl_pointcloud aligned_cld(new_cwipc_pcl_pointcloud());

				transformPointCloud(*configCopy.cameraData[i].cloud, *aligned_cld, *configCopy.cameraData[i].trafo);

				cloud2file(aligned_cld, "pcl_aligned_" + configCopy.cameraData[i].serial + ext);
				cloud2file(configCopy.cameraData[i].cloud, "pcl_original_" + configCopy.cameraData[i].serial + ext);
			}
		}
        else std::cout << key << std::endl;
	};
}


int main(int argc, char * argv[]) try
{
	// Create a simple OpenGL window for rendering:
	window_util app(2560, 1440, "RealSense Multicamera Capturing");
	// Construct a capturing object
	RS2Capture multiframe;

	// register callbacks to allow manipulation of the PointCloud
    register_glfw_callbacks(&app, &multiframe);

	int frame_num = 0;
	uint64_t time = 0;
	Eigen::Vector4f newcenter;
	Eigen::Vector4f deltacenter;

	printhelp();

    if (multiframe.configuration.cameraData.size() < 1) {
        // no camera connected
        if (load_data() && configCopy.cameraData.size() > 0) {
            loaded_mode = true;
            align_mode = true;
			if (aligncamera < 0 || aligncamera >= configCopy.cameraData.size())
				aligncamera = 0;
		}
		else {
			std::cerr << "\npcl_align: Sorry: No cameras connected and no data to load\n\n";
			return EXIT_FAILURE;
		}
	}
	else
            makeFreezeCopy(&multiframe); // make a still copy of multiFrame's configuration

	while (app) {
		if (!(align_mode || loaded_mode) || life_align) {
			// Here we ask for a pointcloud (the merger of all camera's) and thereby trigger the actual capturing
			mergeded_pc = multiframe.get_pointcloud(&time);

			if (mergeded_pc.get() == NULL) continue;

			// Automatically centre the cloud
			if (!(frame_num++ % CENTERSTEPS)) {
				pcl::compute3DCentroid(*mergeded_pc, newcenter);
				deltacenter = (newcenter - mergedcenter) / CENTERSTEPS;
			}
			mergedcenter += deltacenter;
		}
		if (depth_plane)
			draw_background_planes(&app, &multiframe);

		// NB: draw pointcloud ignores the just obtained pointcloud, as it may want to draw pointclouds of the camera's individually rather than the merged one.
		draw_pointclouds(&app, &multiframe);
	}
	return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
	std::cerr << "pcl_align: Error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception & e)
{
	std::cerr << "pcl_align: " << e.what() << std::endl;
	return EXIT_FAILURE;
}
