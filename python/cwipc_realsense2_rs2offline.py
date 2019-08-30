import ctypes
import ctypes.util
from cwipc.util import CwipcError, CWIPC_API_VERSION, cwipc_tiledsource
from cwipc.util import cwipc_tiledsource_p, cwipc_tiledsource

__all__ = [
    "cwpic_rs2offline"
]

class cwipc_offline_p(ctypes.c_void_p):
    pass
    
_cwipc_realsense2_dll_reference = None

#
# NOTE: the signatures here must match those in cwipc_util/api.h or all hell will break loose
#
def _cwipc_realsense2_dll(libname=None):
    """Load the cwipc_util DLL and assign the signatures (if not already loaded)"""
    global _cwipc_realsense2_dll_reference
    if _cwipc_realsense2_dll_reference: return _cwipc_realsense2_dll_reference
    
    if libname == None:
        libname = ctypes.util.find_library('cwipc_realsense2')
        if not libname:
            raise RuntimeError('Dynamic library cwipc_realsense2 not found')
    assert libname
    _cwipc_realsense2_dll_reference = ctypes.CDLL(libname)
    
    _cwipc_realsense2_dll_reference.cwipc_realsense2.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.c_ulong]
    _cwipc_realsense2_dll_reference.cwipc_realsense2.restype = cwipc_tiledsource_p
    _cwipc_realsense2_dll_reference.cwipc_rs2offline.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.c_ulong]
    _cwipc_realsense2_dll_reference.cwipc_rs2offline.restype = cwipc_offline_p
    _cwipc_realsense2_dll_reference.cwipc_offline_free.argtypes = [cwipc_offline_p]
    _cwipc_realsense2_dll_reference.cwipc_offline_free.restype = None
    _cwipc_realsense2_dll_reference.cwipc_offline_get_source.argtypes = [cwipc_offline_p]
    _cwipc_realsense2_dll_reference.cwipc_offline_get_source.restype = cwipc_tiledsource_p
    _cwipc_realsense2_dll_reference.cwipc_offline_feed.argtypes = [cwipc_offline_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
    _cwipc_realsense2_dll_reference.cwipc_offline_feed.restype = ctypes.c_bool

    return _cwipc_realsense2_dll_reference
        
class cwipc_offline_wrapper:
    def __init__(self, _cwipc_offline):
        if _cwipc_offline != None:
            assert isinstance(_cwipc_offline, cwipc_offline_p)
        self._cwipc_offline = _cwipc_offline
        
    def _as_cwipc_offline_p(self):
        assert self._cwipc_offline
        return self._cwipc_offline
        
    def free(self):
        if self._cwipc_offline:
            _cwipc_realsense2_dll().cwipc_offline_free(self._as_cwipc_offline_p())
        self._cwipc_offline = None

    def get_source(self):
        obj = _cwipc_realsense2_dll().cwipc_offline_get_source(self._as_cwipc_offline_p())
        return cwipc_tiledsource(obj)
        
    def feed(self, camNum, colorBuffer, depthBuffer):
        colorLength = len(colorBuffer)
        if isinstance(colorBuffer, bytearray):
            colorBuffer = (ctypes.c_char * colorLength).from_buffer(colorBuffer)
        colorPtr = ctypes.cast(colorBuffer, ctypes.c_void_p)
        depthLength = len(depthBuffer)
        if isinstance(depthBuffer, bytearray):
            depthBuffer = (ctypes.c_char * depthLength).from_buffer(depthBuffer)
        depthPtr = ctypes.cast(colorBuffer, ctypes.c_void_p)
        rv = _cwipc_realsense2_dll().cwipc_offline_feed(self._as_cwipc_offline_p(), camNum, colorPtr, colorLength, depthPtr, depthLength)
        return rv

def cwpic_rs2offline(conffile):
    """Returns a cwipc_source object that grabs from a realsense2 camera and returns cwipc object on every get() call."""
    errorString = ctypes.c_char_p()
    if conffile:
        conffile = conffile.encode('utf8')
    else:
        conffile = None
    rv = _cwipc_realsense2_dll().cwipc_rs2offline(conffile, ctypes.byref(errorString), CWIPC_API_VERSION)
    if errorString:
        raise CwipcError(errorString.value.decode('utf8'))
    if rv:
        return cwipc_offline_wrapper(rv)
    return None
     
def main():
    offline = cwpic_rs2offline('offline.xml')
    print(f'offline={offline} ptr={offline._cwipc_offline}')
    grabber = offline.get_source()
    print(f'grabber={grabber} ptr={grabber._cwipc_source}')
    if not grabber.available(False):
        print("No pointclouds available")
    else:
        pc = grabber.get()
        if not pc:
            print('Could not read pointcloud from realsense2 offline  grabber')
        points = pc.get_points()
        print('Pointcloud contained %d points' % len(points))
        pc.free()
    grabber.free()
    offline.free()
    
if __name__ == '__main__':
    import sys
    sys.stdin.readline()
    _cwipc_realsense2_dll('/Users/jack/src/VRTogether/cwipc_realsense2/build-xcode/lib/Debug/libcwipc_realsense2.dylib')
    main()
    
    
