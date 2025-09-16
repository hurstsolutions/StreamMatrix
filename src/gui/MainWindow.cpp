#include "MainWindow.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Stream Matrix - Preview");
  resize(1100, 700);

  central_ = new QWidget(this);
  auto* root = new QVBoxLayout(central_);

  // Top controls
  auto* ctrlRow = new QHBoxLayout();
  videoCombo_ = new QComboBox(central_);
  audioCombo_ = new QComboBox(central_);
  refreshBtn_ = new QPushButton("Refresh", central_);

  ctrlRow->addWidget(new QLabel("Video:", central_));
  ctrlRow->addWidget(videoCombo_, 2);
  ctrlRow->addSpacing(12);
  ctrlRow->addWidget(new QLabel("Audio:", central_));
  ctrlRow->addWidget(audioCombo_, 2);
  ctrlRow->addSpacing(12);
  ctrlRow->addWidget(refreshBtn_);

  // Preview group
  auto* previewRow = new QHBoxLayout();
  auto* videoGroup = new QGroupBox("Video Preview", central_);
  auto* audioGroup = new QGroupBox("Audio Meters", central_);

  auto* videoLayout = new QVBoxLayout(videoGroup);
  videoWidget_ = new VideoWidget(videoGroup);
  videoWidget_->setMinimumSize(800, 450);
  videoLayout->addWidget(videoWidget_);

  auto* audioLayout = new QVBoxLayout(audioGroup);
  audioMeters_ = new AudioMeterWidget(audioGroup);
  audioMeters_->setMinimumWidth(220);
  audioLayout->addWidget(audioMeters_, 1);

  previewRow->addWidget(videoGroup, 1);
  previewRow->addWidget(audioGroup, 0);

  previewRow->setStretchFactor(videoGroup, 4);
  previewRow->setStretchFactor(audioGroup, 1);

  root->addLayout(ctrlRow);
  root->addLayout(previewRow, 1);

  setCentralWidget(central_);

  connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
  connect(videoCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSelectionChanged);
  connect(audioCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSelectionChanged);

  populateDeviceLists();
  onSelectionChanged();
}

MainWindow::~MainWindow() {
  preview_.stop();
}

void MainWindow::populateDeviceLists() {
  deviceMgr_.refresh();

  videoCombo_->clear();
  for (int i = 0; i < deviceMgr_.videoCount(); ++i) {
    const auto& d = deviceMgr_.video(i);
    videoCombo_->addItem(d.displayName, QVariant{i});
  }
  if (videoCombo_->count() == 0) {
    videoCombo_->addItem("No video devices found");
  }

  audioCombo_->clear();
  for (int i = 0; i < deviceMgr_.audioCount(); ++i) {
    const auto& d = deviceMgr_.audio(i);
    audioCombo_->addItem(d.displayName, QVariant{i});
  }
  if (audioCombo_->count() == 0) {
    audioCombo_->addItem("No audio devices found");
  }
}

void MainWindow::onRefreshDevices() {
  preview_.stop();
  populateDeviceLists();
  onSelectionChanged();
}

void MainWindow::onSelectionChanged() {
  // Determine selected devices
  const GstDevice* vdev = nullptr;
  const GstDevice* adev = nullptr;

  if (videoCombo_->count() > 0 && deviceMgr_.videoCount() > 0 &&
      videoCombo_->currentIndex() >= 0 &&
      videoCombo_->currentIndex() < deviceMgr_.videoCount()) {
    vdev = deviceMgr_.video(videoCombo_->currentIndex()).device;
  }

  if (audioCombo_->count() > 0 && deviceMgr_.audioCount() > 0 &&
      audioCombo_->currentIndex() >= 0 &&
      audioCombo_->currentIndex() < deviceMgr_.audioCount()) {
    adev = deviceMgr_.audio(audioCombo_->currentIndex()).device;
  }

  preview_.start(vdev, adev, videoWidget_, audioMeters_);
}