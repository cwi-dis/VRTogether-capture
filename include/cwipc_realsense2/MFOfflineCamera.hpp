#ifndef cwipc_realsense_MFOfflineCamera_hpp
#define cwipc_realsense_MFOfflineCamera_hpp
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

#include "defs.h"
#include "cwipc_realsense2/MFCamera.hpp"

#ifndef CWIPC_DLL_ENTRY
#if defined(WIN32) || defined(_WIN32)
#define CWIPC_DLL_ENTRY __declspec(dllimport)
#else
#define CWIPC_DLL_ENTRY 
#endif
#endif

class MFOfflineCamera : public MFCamera {
private:
	MFOfflineCamera(const MFOfflineCamera&);	// Disable copy constructor
	MFOfflineCamera& operator=(const MFOfflineCamera&);	// Disable assignment
public:
	MFOfflineCamera(rs2::context& ctx, MFCaptureConfig& configuration, int _camera_index, MFCameraData& _camData);
	virtual ~MFOfflineCamera();

	void _capture_thread_main();
	bool feed_image_data(void *colorBuffer, size_t colorSize,  void *depthBuffer, size_t depthSize);
private:
	int feed_number;			// Number of times feed_image_data() was called (to simulate frame numbers)
	rs2::frameset current_frameset;
	rs2::software_device dev;
	rs2::software_sensor depth_sensor;
	rs2::software_sensor color_sensor;
	rs2::stream_profile color_stream;
	rs2::stream_profile depth_stream;
	rs2::syncer sync;
};
#endif // cwipc_realsense_MFOfflineCamera_hpp
