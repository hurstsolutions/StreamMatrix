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

void PreviewPipeline::start(const GstDevice* video_dev,
                            const GstDevice* audio_dev,
                            VideoWidget* video_widget,
                            AudioMeterWidget* meters) {
  stop();  // Clean previous pipeline

  videoWidget_ = video_widget;
  meters_ = meters;

  // 1. CREATE ALL ELEMENTS
  // ======================
  pipeline_ = gst_pipeline_new("preview-pipeline");

  // Video elements
  GstElement* vsrc = elementFromDevice(video_dev, "vsrc");
  if (!vsrc) {
    vsrc = gst_element_factory_make("videotestsrc", "vsrc");
  }
  GstElement* vqueue = gst_element_factory_make("queue", "vqueue");
  GstElement* vconv = gst_element_factory_make("videoconvert", "vconv");
  videoSink_ = gst_element_factory_make("glimagesink", "vsink");
  if (!videoSink_) {
    videoSink_ = gst_element_factory_make("autovideosink", "vsink");
  }

  // Audio elements
  GstElement* asrc = elementFromDevice(audio_dev, "asrc");
  if (!asrc) {
    asrc = gst_element_factory_make("audiotestsrc", "asrc");
  }
  GstElement* capture_queue = gst_element_factory_make("queue", "capture_queue");
  GstElement* aconv = gst_element_factory_make("audioconvert", "aconv");
  GstElement* ares = gst_element_factory_make("audioresample", "ares");
  atee_ = gst_element_factory_make("tee", "atee");
  GstElement* meter_queue = gst_element_factory_make("queue", "meter_queue");
  level_ = gst_element_factory_make("level", "level");
  GstElement* asink = gst_element_factory_make("fakesink", "asink");
  GstElement* mon_queue = gst_element_factory_make("queue", "mon_queue");
  GstElement* monitor = gst_element_factory_make("autoaudiosink", "monitor");

  // Configure elements
  g_object_set(level_, "interval", guint64(50000000), "post-messages", TRUE, nullptr);
  g_object_set(capture_queue, "leaky", 2, "max-size-buffers", 0, "max-size-time", 0, nullptr);
  g_object_set(asink, "sync", FALSE, nullptr);
  g_object_set(videoSink_, "sync", FALSE, nullptr);

  // 2. ADD ALL ELEMENTS TO THE PIPELINE (ONCE!)
  // ===========================================
  gst_bin_add_many(GST_BIN(pipeline_),
                   vsrc, vqueue, vconv, videoSink_,
                   asrc, capture_queue, aconv, ares, atee_,
                   meter_queue, level_, asink,
                   mon_queue, monitor,
                   nullptr);

  // 3. LINK THE ELEMENTS
  // ====================
  // Link video branch
  if (!gst_element_link_many(vsrc, vqueue, vconv, videoSink_, nullptr)) {
    qWarning() << "Failed to link video elements";
  }

  // Link main audio branch up to the tee
  if (!gst_element_link_many(asrc, capture_queue, aconv, ares, atee_, nullptr)) {
    qWarning() << "Failed to link main audio chain";
  }

  // Link meter branch
  if (!gst_element_link_many(meter_queue, level_, asink, nullptr)) {
    qWarning() << "Failed to link meter branch";
  }

  // Link monitor branch
  if (!gst_element_link(mon_queue, monitor)) {
    qWarning() << "Failed to link monitor branch";
  }

  // Request tee pads and link them to the branches
  atee_src1_ = gst_element_request_pad_simple(atee_, "src_%u");
  GstPad* meter_sink_pad = gst_element_get_static_pad(meter_queue, "sink");
  gst_pad_link(atee_src1_, meter_sink_pad);
  gst_object_unref(meter_sink_pad);

  atee_src2_ = gst_element_request_pad_simple(atee_, "src_%u");
  GstPad* mon_sink_pad = gst_element_get_static_pad(mon_queue, "sink");
  gst_pad_link(atee_src2_, mon_sink_pad);
  gst_object_unref(mon_sink_pad);

  // 4. START THE PIPELINE
  // =====================
  bus_ = gst_element_get_bus(pipeline_);
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  setOverlayIfPossible();
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
  char* structure_string = gst_structure_to_string(s);
  qDebug() << "Full Message Structure: " << structure_string;
  g_free(structure_string);
  const GValue* peaks = gst_structure_get_value(s, "peak");
  if(!peaks) return;
  const char* type_name = G_VALUE_TYPE_NAME(peaks);
  if(!type_name) return;
  qDebug() << "PEAK TYPE " << type_name;


  QVector<float> dbLevels;
  int n = 0;
  if (strcmp(type_name, "GValueList") == 0){
    qDebug() << "In ValueList logic";
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
  } else if (strcmp(type_name, "GValueArray") == 0){
    qDebug() << "In ValueArray logic";
    int n = gst_value_array_get_size(peaks);
    dbLevels.reserve(n);
    for(int i = 0; i < n; ++i){
      const GValue* v = gst_value_array_get_value(peaks, i);
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
    qDebug() << type_name << " didn't match ValueList or ArrayValue";
    return;
  }

  if (meters_) {
    meters_->setPeakLevels(dbLevels);
    qDebug() << "Setting meters_ " << dbLevels;
  }
  else{
    qDebug() << "No meters_ to set";
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