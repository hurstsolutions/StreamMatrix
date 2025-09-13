#pragma once
#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include "gui/AudioMeterWidget.h"
#include "gui/VideoWidget.h"
#include "pipeline/DeviceManager.h"
#include "pipeline/PreviewPipeline.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

private slots:
  void onRefreshDevices();
  void onSelectionChanged();

private:
  void populateDeviceLists();

  QWidget* central_{nullptr};
  QComboBox* videoCombo_{nullptr};
  QComboBox* audioCombo_{nullptr};
  QPushButton* refreshBtn_{nullptr};
  VideoWidget* videoWidget_{nullptr};
  AudioMeterWidget* audioMeters_{nullptr};

  DeviceManager deviceMgr_;
  PreviewPipeline preview_;
};