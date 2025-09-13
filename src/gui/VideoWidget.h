#pragma once
#include <QWidget>

class VideoWidget : public QWidget {
  Q_OBJECT
public:
  explicit VideoWidget(QWidget* parent = nullptr);

  // Returns a platform window handle suitable for GstVideoOverlay
  uintptr_t gstWindowHandle() const;

protected:
  void showEvent(QShowEvent* e) override;

private:
  void ensureNative();
};