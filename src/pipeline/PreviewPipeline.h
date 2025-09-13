#pragma once
#include <QPointer>
#include <QTimer>
#include <gst/gst.h>
#include "gui/AudioMeterWidget.h"
#include "gui/VideoWidget.h"

class PreviewPipeline : public QObject {
  Q_OBJECT
public:
  PreviewPipeline();
  ~PreviewPipeline() override;

  void start(const GstDevice* videoDev,
             const GstDevice* audioDev,
             VideoWidget* videoWidget,
             AudioMeterWidget* meters);

  void stop();

private slots:
  void pollBus();

private:
  GstElement* atee_{nullptr};
  GstPad* atee_src1_{nullptr};
  GstPad* atee_src2_{nullptr};
  GstElement* pipeline_{nullptr};
  GstElement* videoSink_{nullptr};
  GstElement* level_{nullptr};
  GstBus* bus_{nullptr};

  QPointer<VideoWidget> videoWidget_;
  QPointer<AudioMeterWidget> meters_;
  QTimer busTimer_;

  void setOverlayIfPossible();
  void handleLevelMessage(GstMessage* msg);
};