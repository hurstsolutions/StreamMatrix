#include "AudioMeterWidget.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

static float dbToNorm(float db) {
  if (!std::isfinite(db)) return 0.f;
  if (db <= -60.f) return 0.f;
  if (db >= 0.f) return 1.f;
  return (db + 60.f) / 60.f;
}

AudioMeterWidget::AudioMeterWidget(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(200);
}

void AudioMeterWidget::setPeakLevels(const QVector<float>& dbLevels) {
  {
    QMutexLocker lock(&mtx_);
    levelsDb_ = dbLevels;
  }
  update();
}

void AudioMeterWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(20, 20, 20));

  QVector<float> levels;
  {
    QMutexLocker lock(&mtx_);
    levels = levelsDb_;
  }

  if (levels.isEmpty()) {
    p.setPen(Qt::gray);
    p.drawText(rect(), Qt::AlignCenter, "No audio");
    return;
  }

  const int n = levels.size();
  const int spacing = 6;
  const int barWidth = std::max(10, (width() - (n + 1) * spacing) / std::max(1, n));
  int x = spacing;

  for (int i = 0; i < n; ++i) {
    float norm = dbToNorm(levels[i]);  // 0..1
    int h = static_cast<int>(norm * (height() - 30));
    QRect bar(x, height() - h - 10, barWidth, h);

    // Background bar
    p.fillRect(QRect(x, 10, barWidth, height() - 20), QColor(60, 60, 60));

    // Color zones: green (-60..-18), yellow (-18..-6), red (-6..0)
    int totalH = height() - 20;
    int greenH = totalH * 42 / 60;   // -60..-18 -> 42/60
    int yellowH = totalH * 12 / 60;  // -18..-6 -> 12/60
    int redH = totalH - greenH - yellowH;

    QRect redRect(x, 10+ (totalH - (redH + yellowH + greenH)), barWidth, redH);
    QRect yellowRect(x, redRect.bottom() + 1, barWidth, yellowH);
    QRect greenRect(x, yellowRect.bottom() + 1, barWidth, greenH);



    // Draw filled portion with clipping
    p.save();
    p.setClipRect(bar);
    p.fillRect(greenRect, QColor(0, 180, 0));
    p.fillRect(yellowRect, QColor(230, 200, 0));
    p.fillRect(redRect, QColor(220, 40, 40));
    p.restore();

    // Channel label
    p.setPen(Qt::lightGray);
    p.drawText(QRect(x, height() - 10, barWidth, 10), Qt::AlignCenter,
               QString::number(i + 1));

    x += barWidth + spacing;
  }

  // dB scale ticks
  p.setPen(QColor(150, 150, 150));
  p.drawText(4, 14, "0 dB");
  p.drawText(4, height() - 4, "-60 dB");
}