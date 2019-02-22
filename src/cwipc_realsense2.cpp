
#if defined(WIN32) || defined(_WIN32)
#define _CWIPC_REALSENSE2_EXPORT __declspec(dllexport)
#endif

#include "cwipc_util/api_pcl.h"
#include "cwipc_util/api.h"
#include "cwipc_realsense2/api.h"

#include "cwipc_realsense2/multiFrame.hpp"

class cwipc_source_realsense2_impl : public cwipc_source {
private:
    multiFrame *m_grabber;
public:
    cwipc_source_realsense2_impl() : m_grabber(NULL){ m_grabber = new multiFrame(); }

    ~cwipc_source_realsense2_impl() {
        delete m_grabber;
    }

    void free() {
        delete m_grabber;
    }

    cwipc* get() {
        if (m_grabber == NULL) return NULL;
        uint64_t timestamp;
        cwipc_pcl_pointcloud pc = m_grabber->get_pointcloud(&timestamp);
        if (pc == NULL) return NULL;
        return cwipc_from_pcl(pc, timestamp, NULL);
    }
};

//
// C-compatible entry points
//

cwipc_source* cwipc_realsense2(char **errorMessage)
{
    return new cwipc_source_realsense2_impl();
}

cwipc* cwipc_source_get(cwipc_source *src)
{
    return src->get();
}

void cwipc_source_free(cwipc_source *src)
{
    src->free();
}
