// OpenCL-based GPU-accelerated version of Task_Wavelet.

#pragma once
#include "wavelet.h"

namespace FStack {

class Task_Wavelet_OpenCL: public Task_Wavelet
{
public:
  Task_Wavelet_OpenCL(std::shared_ptr<ImgTask> input, bool inverse);

  virtual bool uses_opencl() { return true; }

private:
  virtual void task();
};

} // namespace FStack
