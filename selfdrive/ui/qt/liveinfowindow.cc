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

  static int oldSteerRatio;
  static int  oldSteerActuatorDelay;
  static int i = 0;

  const auto gm = (*s.sm)["getmishka"].getGetmishka();
  state = gm.getState(); 
  delayKoef = gm.getDelayKoef();
  ratioKoef = gm.getRatioKoef();

  const auto lp = (*s.sm)["liveParameters"].getLiveParameters();
    steerRatio = lp.getSteerRatio();
    //steerRatio = ratioKoef;

  //const auto cp = (*s.sm)["carParams"].getCarParams();
    // steerActuatorDelay = cp.getSteerActuatorDelay();
    steerActuatorDelay = delayKoef;

  

  //angleOffset = int(round(s.sm['liveParameters'].angleOffsetDeg * 10))
  //steerRatio = int(round(s.sm['liveParameters'].steerRatio * 10))    

  // const auto dev = (*s.sm)["deviceState"].getDeviceState();
  // float hottest = -999.f;
  // for (float t : dev.getCpuTempC()) hottest = std::max(hottest, t);

  // cpu  = qRound(hottest);
  // batt = int(dev.getBatteryPercent());
  
  // if ((oldSteerRatio != steerRatio) || (oldSteerActuatorDelay != steerActuatorDelay)) {
  //   update();
  // }
  if (i%10 == 0){
    update();                       // перерисовать
  }
  i++;
    oldSteerRatio = steerRatio;
  oldSteerActuatorDelay = steerActuatorDelay;
}

void LiveInfoWindow::paintEvent(QPaintEvent *) {
  
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(QFont("Inter", 10, QFont::Bold));

  // строки
  QString sadStr  = QString("delay  : %1").arg(steerActuatorDelay);
  QString srStr  = QString("ratio  : %1").arg(steerRatio);

  // координаты (правый-верхний угол)
  const int margin = 100;
  const int lineH  = p.fontMetrics().height() + 10;
  int x = width()  - p.fontMetrics().horizontalAdvance(sadStr) - margin;
  int y = margin + lineH;

  // лёгкая тень
  p.setPen(QColor(0,0,0,160));
  p.drawText(x+2, y+2, sadStr);
  p.drawText(x+2, y+2+lineH, srStr);
  //p.drawText(x+2, y+2+lineH, battStr);

  // основной белый текст
  if (state == sadChangeState)
    p.setPen(Qt::green);
  else if (state == normalState)
    p.setPen(Qt::white);
  p.drawText(x, y, sadStr);

  if (state == srChangeState)  
    p.setPen(Qt::green);
  else if (state == normalState)
    p.setPen(Qt::white);
  p.drawText(x, y+lineH, srStr);
}