#pragma once
#include <QMutex>
#include <QVector>
#include <QWidget>

class AudioMeterWidget : public QWidget {
  Q_OBJECT
public:
  explicit AudioMeterWidget(QWidget* parent = nullptr);

  // levels in dBFS per channel (e.g., -60..0). Use -INF (~-1000) for silence.
  void setPeakLevels(const QVector<float>& dbLevels);

  QSize sizeHint() const override { return {220, 200}; }

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QVector<float> levelsDb_;  // per-channel peaks in dBFS
  QMutex mtx_;
};