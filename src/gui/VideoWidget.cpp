#include "VideoWidget.h"
#include <QWindow>

VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_NativeWindow, true);
  setAttribute(Qt::WA_PaintOnScreen, false);
  setAutoFillBackground(true);
  setStyleSheet("background-color: black;");
}

void VideoWidget::ensureNative() {
  winId();  // forces creation of a native window handle
}

uintptr_t VideoWidget::gstWindowHandle() const {
  return static_cast<uintptr_t>(winId());
}

void VideoWidget::showEvent(QShowEvent* e) {
  ensureNative();
  QWidget::showEvent(e);
}