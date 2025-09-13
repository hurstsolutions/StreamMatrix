#include "PreviewPipeline.h"
#include <QDebug>
#include <gst/video/videooverlay.h>
#include <gst/gstmessage.h>
#include <gst/gst.h>

PreviewPipeline::PreviewPipeline() {
  busTimer_.setInterval(30);
  connect(&busTimer_, &QTimer::timeout, this, &PreviewPipeline::pollBus);
}

PreviewPipeline::~PreviewPipeline() {
  stop();
}

void PreviewPipeline::stop() {
  busTimer_.stop();
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
  }
  if(atee_){
    if(atee_src1_){
      gst_element_release_request_pad(atee_, atee_src1_);
      gst_object_unref(atee_src1_);
      atee_src1_ = nullptr;
    }
    if(atee_src2_){
      gst_element_release_request_pad(atee_, atee_src2_);
      gst_object_unref(atee_src2_);
      atee_src2_ = nullptr;
    }
  }
  if (bus_) {
    gst_object_unref(bus_);
    bus_ = nullptr;
  }
  if (pipeline_) {
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
  videoSink_ = nullptr;
  level_ = nullptr;
}

static GstElement* elementFromDevice(const GstDevice* dev,
                                     const char* nameIfCreated) {
  if (dev) {
    return gst_device_create_element(const_cast<GstDevice*>(dev), nameIfCreated);
  }
  return nullptr;
}

void PreviewPipeline::start(const GstDevice* videoDev,
                            const GstDevice* audioDev,
                            VideoWidget* videoWidget,
                            AudioMeterWidget* meters) {
  stop();  // clean previous pipeline

  videoWidget_ = videoWidget;
  meters_ = meters;

  // Elements
  pipeline_ = gst_pipeline_new("preview-pipeline");

  // Video branch
  GstElement* vsrc = elementFromDevice(videoDev, "vsrc");
  if (!vsrc) {
    vsrc = gst_element_factory_make("videotestsrc", "vsrc");
    g_object_set(vsrc, "pattern", 0, nullptr);  // SMPTE color bars if desired
  }
  GstElement* vqueue = gst_element_factory_make("queue", "vqueue");
  GstElement* vconv = gst_element_factory_make("videoconvert", "vconv");
  videoSink_ = gst_element_factory_make("glimagesink", "vsink");
  if (!videoSink_) {
    // Fallback sink if GL not present
    videoSink_ = gst_element_factory_make("autovideosink", "vsink");
  }
  g_object_set(videoSink_, "sync", FALSE, nullptr);

  // Audio branch
  GstElement* asrc = elementFromDevice(audioDev, "asrc");
  if (!asrc) {
    asrc = gst_element_factory_make("audiotestsrc", "asrc");
    g_object_set(asrc, "is-live", TRUE, nullptr);
  }
  GstElement* acap_queue = gst_element_factory_make("queue", "acap_queue");

  GstElement* aconv = gst_element_factory_make("audioconvert", "aconv");
  GstElement* ares = gst_element_factory_make("audioresample", "ares");
  level_ = gst_element_factory_make("level", "level");
  atee_ = gst_element_factory_make("tee", "atee");
  GstElement* aqueue1 = gst_element_factory_make("queue", "aqueue1");
  GstElement* aqueue2 = gst_element_factory_make("queue", "aqueue2");
  GstElement* asink = gst_element_factory_make("fakesink", "asink");
  GstElement* mon = gst_element_factory_make("autoaudiosink", "mon");
  g_object_set(level_,
               "interval", guint64(50000000),  // 50 ms
               "post-messages", TRUE,
               nullptr);
  g_object_set(asink, "sync", FALSE, nullptr);
  g_object_set(acap_queue, "leaky", 2, "max-size-buffers", 0, "max-size-time", 0, nullptr);
  gst_bin_add_many(GST_BIN(pipeline_), asrc, acap_queue, aconv, ares, atee_, aqueue1, level_, asink, aqueue2, mon, nullptr);
  gst_element_link_many(asrc, acap_queue, aconv, ares, atee_, nullptr);

  // Add to pipeline
  gst_bin_add_many(GST_BIN(pipeline_), vsrc, vqueue, vconv, videoSink_, nullptr);
  gst_bin_add_many(GST_BIN(pipeline_), asrc, aconv, ares, atee_, aqueue1, level_, asink, aqueue2, mon, nullptr);

  // Link video
  if (!gst_element_link_many(vsrc, vqueue, vconv, videoSink_, nullptr)) {
    qWarning() << "Failed to link video elements";
  }

  // Link audio
  if (!gst_element_link_many(aqueue1, level_, asink, nullptr)) {
    qWarning() << "Failed to link meter branch";
  }

  if (!gst_element_link_many(asrc, aconv, ares, atee_, nullptr)){
    qWarning() << "Failed to link audio elements";
  }

  if (!gst_element_link_many(aqueue2, mon, nullptr)){
    qWarning() << "Failed to link monitor branch";
  }

  atee_src1_ = gst_element_request_pad_simple(atee_, "src_%u");
  GstPad* sink1 = gst_element_get_static_pad(aqueue1, "sink");
  gst_pad_link(atee_src1_, sink1);
  gst_object_unref(sink1);
  atee_src2_ = gst_element_request_pad_simple(atee_, "src_%u");
  GstPad* sink2 = gst_element_get_static_pad(aqueue2, "sink");
  gst_pad_link(atee_src2_, sink2);
  gst_object_unref(sink2);

  // Bus for messages
  bus_ = gst_element_get_bus(pipeline_);

  // Set the pipeline to playing
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);

  // Set overlay ASAP (some sinks also send prepare-window-handle)
  setOverlayIfPossible();

  // Start polling bus for errors and level messages
  busTimer_.start();
}

void PreviewPipeline::setOverlayIfPossible() {
  if (!videoSink_ || !GST_IS_VIDEO_OVERLAY(videoSink_)) return;
  if (!videoWidget_) return;

  // Ensure the widget has a native handle
  uintptr_t handle = videoWidget_->gstWindowHandle();
  if (handle != 0) {
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink_),
                                        static_cast<guintptr>(handle));
  }
}

void PreviewPipeline::handleLevelMessage(GstMessage* msg) {
  qDebug() << "LEVEL MSG";
  const GstStructure* s = gst_message_get_structure(msg);
  if(!s) return;
  const GValue* peaks = gst_structure_get_value(s, "peak");
  qDebug() << "PEAK TYPE " << G_VALUE_TYPE_NAME(peaks);
  if(!peaks) return;

  QVector<float> dbLevels;
  int n = 0;
  if (GST_VALUE_HOLDS_LIST(peaks)){
    n = gst_value_list_get_size(peaks);
    dbLevels.reserve(n);
    for(int i = 0; i < n; ++i){
      const GValue* v = gst_value_list_get_value(peaks, i);
      if (G_VALUE_HOLDS_DOUBLE(v)){
        dbLevels.push_back(static_cast<float>(g_value_get_double(v)));
      } else if (G_VALUE_HOLDS_FLOAT(v)){
        dbLevels.push_back(g_value_get_float(v));
      } else{
        dbLevels.push_back(-60.f);
      }
    }
  } else if (GST_VALUE_HOLDS_ARRAY(peaks)){
    int n = gst_value_array_get_size(peaks);
    dbLevels.reserve(n);
    for(int i = 0; i < n; ++i){
      const GValue* v = gst_value_list_get_value(peaks, i);
      if (G_VALUE_HOLDS_DOUBLE(v)){
        dbLevels.push_back(static_cast<float>(g_value_get_double(v)));
      } else if (G_VALUE_HOLDS_FLOAT(v)){
        dbLevels.push_back(g_value_get_float(v));
      } else{
        dbLevels.push_back(-60.f);
      }
    }
  } 
  else{
    return;
  }

  if (meters_) {
    meters_->setPeakLevels(dbLevels);
  }
}

void PreviewPipeline::pollBus() {
  if (!bus_) return;

  while (true) {
    GstMessage* msg = gst_bus_pop(bus_);
    if (!msg) break;

    switch (GST_MESSAGE_TYPE(msg)) {
      case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        qWarning() << "GStreamer error:" << (err ? err->message : "unknown");
        if (dbg) {
          qWarning() << "Debug:" << dbg;
          g_free(dbg);
        }
        if (err) g_error_free(err);
        break;
      }
      case GST_MESSAGE_ELEMENT: {
        const GstStructure* s = gst_message_get_structure(msg);
        if (s && gst_structure_has_name(s, "prepare-window-handle")) {
          setOverlayIfPossible();
        } else if (s && gst_structure_has_name(s, "level")) {
          handleLevelMessage(msg);
        }
        break;
      }
      default:
        break;
    }

    gst_message_unref(msg);
  }
}