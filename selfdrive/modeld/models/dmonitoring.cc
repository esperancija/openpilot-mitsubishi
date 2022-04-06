#include <cstring>

#include "libyuv.h"

#include "selfdrive/common/mat.h"
#include "selfdrive/common/modeldata.h"
#include "selfdrive/common/params.h"
#include "selfdrive/common/timing.h"
#include "selfdrive/hardware/hw.h"

#include "selfdrive/modeld/models/dmonitoring.h"

constexpr int MODEL_WIDTH = 320;
constexpr int MODEL_HEIGHT = 640;

template <class T>
static inline T *get_buffer(std::vector<T> &buf, const size_t size) {
  if (buf.size() < size) buf.resize(size);
  return buf.data();
}

static inline void init_yuv_buf(std::vector<uint8_t> &buf, const int width, int height) {
  uint8_t *y = get_buffer(buf, width * height * 3 / 2);
  uint8_t *u = y + width * height;
  uint8_t *v = u + (width / 2) * (height / 2);

  // needed on comma two to make the padded border black
  // equivalent to RGB(0,0,0) in YUV space
  memset(y, 16, width * height);
  memset(u, 128, (width / 2) * (height / 2));
  memset(v, 128, (width / 2) * (height / 2));
}

void dmonitoring_init(DMonitoringModelState* s) {
  s->is_rhd = Params().getBool("IsRHD");
  for (int x = 0; x < std::size(s->tensor); ++x) {
    s->tensor[x] = (x - 128.f) * 0.0078125f;
  }
  init_yuv_buf(s->resized_buf, MODEL_WIDTH, MODEL_HEIGHT);

#ifdef USE_ONNX_MODEL
  s->m = new ONNXModel("../../models/dmonitoring_model.onnx", &s->output[0], OUTPUT_SIZE, USE_DSP_RUNTIME);
#else
  s->m = new SNPEModel("../../models/dmonitoring_model_q.dlc", &s->output[0], OUTPUT_SIZE, USE_DSP_RUNTIME);
#endif

  s->m->addCalib(s->calib, CALIB_LEN);
}

static inline auto get_yuv_buf(std::vector<uint8_t> &buf, const int width, int height) {
  uint8_t *y = get_buffer(buf, width * height * 3 / 2);
  uint8_t *u = y + width * height;
  uint8_t *v = u + (width /2) * (height / 2);
  return std::make_tuple(y, u, v);
}

struct Rect {int x, y, w, h;};
void crop_yuv(uint8_t *raw, int width, int height, uint8_t *y, uint8_t *u, uint8_t *v, const Rect &rect) {
  uint8_t *raw_y = raw;
  uint8_t *raw_u = raw_y + (width * height);
  uint8_t *raw_v = raw_u + ((width / 2) * (height / 2));
  for (int r = 0; r < rect.h / 2; r++) {
    memcpy(y + 2 * r * rect.w, raw_y + (2 * r + rect.y) * width + rect.x, rect.w);
    memcpy(y + (2 * r + 1) * rect.w, raw_y + (2 * r + rect.y + 1) * width + rect.x, rect.w);
    memcpy(u + r * (rect.w / 2), raw_u + (r + (rect.y / 2)) * width / 2 + (rect.x / 2), rect.w / 2);
    memcpy(v + r * (rect.w / 2), raw_v + (r + (rect.y / 2)) * width / 2 + (rect.x / 2), rect.w / 2);
  }
}

DMonitoringResult dmonitoring_eval_frame(DMonitoringModelState* s, void* stream_buf, int width, int height, float *calib) {
  Rect crop_rect;
  if (width == TICI_CAM_WIDTH) {
    const int cropped_height = tici_dm_crop::width / 1.33;
    crop_rect = {width / 2 - tici_dm_crop::width / 2 + tici_dm_crop::x_offset,
                 height / 2 - cropped_height / 2 + tici_dm_crop::y_offset,
                 cropped_height / 2,
                 cropped_height};
    if (!s->is_rhd) {
      crop_rect.x += tici_dm_crop::width - crop_rect.w;
    }
  } else {
    const int adapt_width = 372;
    crop_rect = {0, 0, adapt_width, height};
    if (!s->is_rhd) {
      crop_rect.x += width - crop_rect.w;
    }
  }

  int resized_width = MODEL_WIDTH;
  int resized_height = MODEL_HEIGHT;

  auto [cropped_y, cropped_u, cropped_v] = get_yuv_buf(s->cropped_buf, crop_rect.w, crop_rect.h);
  if (!s->is_rhd) {
    crop_yuv((uint8_t *)stream_buf, width, height, cropped_y, cropped_u, cropped_v, crop_rect);
  } else {
    auto [mirror_y, mirror_u, mirror_v] = get_yuv_buf(s->premirror_cropped_buf, crop_rect.w, crop_rect.h);
    crop_yuv((uint8_t *)stream_buf, width, height, mirror_y, mirror_u, mirror_v, crop_rect);
    libyuv::I420Mirror(mirror_y, crop_rect.w,
                       mirror_u, crop_rect.w / 2,
                       mirror_v, crop_rect.w / 2,
                       cropped_y, crop_rect.w,
                       cropped_u, crop_rect.w / 2,
                       cropped_v, crop_rect.w / 2,
                       crop_rect.w, crop_rect.h);
  }

  auto [resized_buf, resized_u, resized_v] = get_yuv_buf(s->resized_buf, resized_width, resized_height);
  uint8_t *resized_y = resized_buf;
  libyuv::FilterMode mode = libyuv::FilterModeEnum::kFilterBilinear;
  if (Hardware::TICI()) {
    libyuv::I420Scale(cropped_y, crop_rect.w,
                    cropped_u, crop_rect.w / 2,
                    cropped_v, crop_rect.w / 2,
                    crop_rect.w, crop_rect.h,
                    resized_y, resized_width,
                    resized_u, resized_width / 2,
                    resized_v, resized_width / 2,
                    resized_width, resized_height,
                    mode);
  } else {
    const int source_height = 0.7*resized_height;
    const int extra_height = (resized_height - source_height) / 2;
    const int extra_width = (resized_width - source_height / 2) / 2;
    const int source_width = source_height / 2 + extra_width;
    libyuv::I420Scale(cropped_y, crop_rect.w,
                    cropped_u, crop_rect.w / 2,
                    cropped_v, crop_rect.w / 2,
                    crop_rect.w, crop_rect.h,
                    resized_y + extra_height * resized_width, resized_width,
                    resized_u + extra_height / 2 * resized_width / 2, resized_width / 2,
                    resized_v + extra_height / 2 * resized_width / 2, resized_width / 2,
                    source_width, source_height,
                    mode);
  }

  int yuv_buf_len = (MODEL_WIDTH/2) * (MODEL_HEIGHT/2) * 6; // Y|u|v -> y|y|y|y|u|v
  float *net_input_buf = get_buffer(s->net_input_buf, yuv_buf_len);
  // one shot conversion, O(n) anyway
  // yuvframe2tensor, normalize
  for (int r = 0; r < MODEL_HEIGHT/2; r++) {
    for (int c = 0; c < MODEL_WIDTH/2; c++) {
      // Y_ul
      net_input_buf[(r*MODEL_WIDTH/2) + c + (0*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_y[(2*r)*resized_width + 2*c]];
      // Y_dl
      net_input_buf[(r*MODEL_WIDTH/2) + c + (1*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_y[(2*r+1)*resized_width + 2*c]];
      // Y_ur
      net_input_buf[(r*MODEL_WIDTH/2) + c + (2*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_y[(2*r)*resized_width + 2*c+1]];
      // Y_dr
      net_input_buf[(r*MODEL_WIDTH/2) + c + (3*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_y[(2*r+1)*resized_width + 2*c+1]];
      // U
      net_input_buf[(r*MODEL_WIDTH/2) + c + (4*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_u[r*resized_width/2 + c]];
      // V
      net_input_buf[(r*MODEL_WIDTH/2) + c + (5*(MODEL_WIDTH/2)*(MODEL_HEIGHT/2))] = s->tensor[resized_v[r*resized_width/2 + c]];
    }
  }

  //printf("preprocess completed. %d \n", yuv_buf_len);
  //FILE *dump_yuv_file = fopen("/tmp/rawdump.yuv", "wb");
  //fwrite(resized_buf, yuv_buf_len, sizeof(uint8_t), dump_yuv_file);
  //fclose(dump_yuv_file);

  // *** testing ***
  // idat = np.frombuffer(open("/tmp/inputdump.yuv", "rb").read(), np.float32).reshape(6, 160, 320)
  // imshow(cv2.cvtColor(tensor_to_frames(idat[None]/0.0078125+128)[0], cv2.COLOR_YUV2RGB_I420))

  //FILE *dump_yuv_file2 = fopen("/tmp/inputdump.yuv", "wb");
  //fwrite(net_input_buf, MODEL_HEIGHT*MODEL_WIDTH*3/2, sizeof(float), dump_yuv_file2);
  //fclose(dump_yuv_file2);

  double t1 = millis_since_boot();
  s->m->addImage(net_input_buf, yuv_buf_len);
  for (int i = 0; i < CALIB_LEN; i++) {
    s->calib[i] = calib[i];
  }
  s->m->execute();
  double t2 = millis_since_boot();

  DMonitoringResult ret = {0};
  for (int i = 0; i < 3; ++i) {
    ret.face_orientation[i] = s->output[i] * REG_SCALE;
    ret.face_orientation_meta[i] = exp(s->output[6 + i]);
  }
  for (int i = 0; i < 2; ++i) {
    ret.face_position[i] = s->output[3 + i] * REG_SCALE;
    ret.face_position_meta[i] = exp(s->output[9 + i]);
  }
  for (int i = 0; i < 4; ++i) {
    ret.ready_prob[i] = sigmoid(s->output[39 + i]);
  }
  for (int i = 0; i < 2; ++i) {
    ret.not_ready_prob[i] = sigmoid(s->output[43 + i]);
  }
  ret.face_prob = sigmoid(s->output[12]);
  ret.left_eye_prob = sigmoid(s->output[21]);
  ret.right_eye_prob = sigmoid(s->output[30]);
  ret.left_blink_prob = sigmoid(s->output[31]);
  ret.right_blink_prob = sigmoid(s->output[32]);
  ret.sg_prob = sigmoid(s->output[33]);
  ret.poor_vision = sigmoid(s->output[34]);
  ret.partial_face = sigmoid(s->output[35]);
  ret.distracted_pose = sigmoid(s->output[36]);
  ret.distracted_eyes = sigmoid(s->output[37]);
  ret.occluded_prob = sigmoid(s->output[38]);
  ret.dsp_execution_time = (t2 - t1) / 1000.;
  return ret;
}

void dmonitoring_publish(PubMaster &pm, uint32_t frame_id, const DMonitoringResult &res, float execution_time, kj::ArrayPtr<const float> raw_pred) {
  // make msg
  MessageBuilder msg;
  auto framed = msg.initEvent().initDriverState();
  framed.setFrameId(frame_id);
  framed.setModelExecutionTime(execution_time);
  framed.setDspExecutionTime(res.dsp_execution_time);

  framed.setFaceOrientation(res.face_orientation);
  framed.setFaceOrientationStd(res.face_orientation_meta);
  framed.setFacePosition(res.face_position);
  framed.setFacePositionStd(res.face_position_meta);
  framed.setFaceProb(res.face_prob);
  framed.setLeftEyeProb(res.left_eye_prob);
  framed.setRightEyeProb(res.right_eye_prob);
  framed.setLeftBlinkProb(res.left_blink_prob);
  framed.setRightBlinkProb(res.right_blink_prob);
  framed.setSunglassesProb(res.sg_prob);
  framed.setPoorVision(res.poor_vision);
  framed.setPartialFace(res.partial_face);
  framed.setDistractedPose(res.distracted_pose);
  framed.setDistractedEyes(res.distracted_eyes);
  framed.setOccludedProb(res.occluded_prob);
  framed.setReadyProb(res.ready_prob);
  framed.setNotReadyProb(res.not_ready_prob);
  if (send_raw_pred) {
    framed.setRawPredictions(raw_pred.asBytes());
  }

  pm.send("driverState", msg);
}

void dmonitoring_free(DMonitoringModelState* s) {
  delete s->m;
}
