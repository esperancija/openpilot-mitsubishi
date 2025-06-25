#include "selfdrive/ui/qt/liveinfowindow.h"

#include <QPainter>
#include <QtMath>          // qRound()
#include <algorithm>       // std::max

LiveInfoWindow::LiveInfoWindow(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LiveInfoWindow::updateState(const UIState &s) {
  const auto dev = (*s.sm)["deviceState"].getDeviceState();

  float hottest = -999.f;
  for (float t : dev.getCpuTempC()) hottest = std::max(hottest, t);

  cpu  = qRound(hottest);
  batt = int(dev.getBatteryPercent());
  update();                       // перерисовать
}

void LiveInfoWindow::paintEvent(QPaintEvent *) {
  if (cpu < 0 || batt < 0) return;       // ещё нет данных

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(QFont("Inter", 10, QFont::Bold));

  // строки
  QString cpuStr  = QString("CPU  : %1 °C").arg(cpu);
  QString battStr = QString("Batt : %1 %").arg(batt);

  // координаты (правый-верхний угол)
  const int margin = 32;
  const int lineH  = p.fontMetrics().height() + 10;
  int x = width()  - p.fontMetrics().horizontalAdvance(cpuStr) - margin;
  int y = margin + lineH;

  // лёгкая тень
  p.setPen(QColor(0,0,0,160));
  p.drawText(x+2, y+2, cpuStr);
  //p.drawText(x+2, y+2+lineH, battStr);

  // основной белый текст
  p.setPen(Qt::white);
  p.drawText(x, y, cpuStr);
  // p.drawText(x, y+lineH, battStr);
}