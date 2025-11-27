// Performs image denoising, using the nonlinear wavelet denoising algorithm.

#pragma once
#include "PetteriModular/Core/worker.h"

namespace FStack {

class Task_Denoise: public ImgTask
{
public:
  Task_Denoise(std::shared_ptr<ImgTask> input, float level);

private:
  virtual void task();

  std::shared_ptr<ImgTask> m_input;
  float m_level;
};

} // namespace FStack

