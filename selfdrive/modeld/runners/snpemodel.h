#pragma once

#include <DlContainer/IDlContainer.hpp>
#include <DlSystem/DlError.hpp>
#include <DlSystem/ITensor.hpp>
#include <DlSystem/ITensorFactory.hpp>
#include <DlSystem/IUserBuffer.hpp>
#include <DlSystem/IUserBufferFactory.hpp>
#include <SNPE/SNPE.hpp>
#include <SNPE/SNPEBuilder.hpp>
#include <SNPE/SNPEFactory.hpp>

#include "runmodel.h"

#define USE_CPU_RUNTIME 0
#define USE_GPU_RUNTIME 1
#define USE_DSP_RUNTIME 2

#ifdef USE_THNEED
#include "selfdrive/modeld/thneed/thneed.h"
#endif

class SNPEModel : public RunModel {
public:
  SNPEModel(const char *path, float *loutput, size_t loutput_size, int runtime, bool luse_extra = false);
  void addRecurrent(float *state, int state_size);
  void addTrafficConvention(float *state, int state_size);
  void addCalib(float *state, int state_size);
  void addDesire(float *state, int state_size);
  void addImage(float *image_buf, int buf_size);
  void addExtra(float *image_buf, int buf_size);
  void execute();

#ifdef USE_THNEED
  Thneed *thneed = NULL;
#endif

private:
  std::string model_data;

#if defined(QCOM) || defined(QCOM2)
  zdl::DlSystem::Runtime_t Runtime;
#endif

  // snpe model stuff
  std::unique_ptr<zdl::SNPE::SNPE> snpe;

  // snpe input stuff
  zdl::DlSystem::UserBufferMap inputMap;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> inputBuffer;
  float *input;
  size_t input_size;

  // snpe output stuff
  zdl::DlSystem::UserBufferMap outputMap;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> outputBuffer;
  float *output;
  size_t output_size;

  // extra input stuff
  std::unique_ptr<zdl::DlSystem::IUserBuffer> extraBuffer;
  float *extra;
  size_t extra_size;
  bool use_extra;

  // recurrent and desire
  std::unique_ptr<zdl::DlSystem::IUserBuffer> addExtra(float *state, int state_size, int idx);
  float *recurrent;
  size_t recurrent_size;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> recurrentBuffer;
  float *trafficConvention;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> trafficConventionBuffer;
  float *desire;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> desireBuffer;
  float *calib;
  std::unique_ptr<zdl::DlSystem::IUserBuffer> calibBuffer;
};
