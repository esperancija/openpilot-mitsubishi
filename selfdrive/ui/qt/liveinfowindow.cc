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

  static int i = 0;

  const auto gm = (*s.sm)["getmishka"].getGetmishka();
  menuState = gm.getState(); 
  delayKoef = gm.getDelayKoef();
  ratioKoef = gm.getRatioKoef();

  //const auto lp = (*s.sm)["liveParameters"].getLiveParameters();
    //steerRatio = lp.getSteerRatio();
    steerRatio = ratioKoef;//*5/10;

  //const auto cp = (*s.sm)["carParams"].getCarParams();
    // steerActuatorDelay = cp.getSteerActuatorDelay();
    steerActuatorDelay = delayKoef; // /500;

  

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
}

void LiveInfoWindow::paintEvent(QPaintEvent *) {
  
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(QFont("Inter", 10, QFont::Bold));

  // строки
  QString sadStr  = QString("delay  : %1").arg(steerActuatorDelay/500, 0, 'f', 3);
//  QString srStr  = QString("ratio  : %1").arg(steerRatio, 0, 'f', 1);
  QString srStr  = QString("ratio  : %1").arg(steerRatio*5/10, 0, 'f', 1);

  // координаты (правый-верхний угол)
  const int margin = 100;
  const int lineH  = p.fontMetrics().height() + 10;
  int x = width()  - p.fontMetrics().horizontalAdvance(sadStr) - margin;
  int y = height() - margin - lineH;

  // лёгкая тень
  p.setPen(QColor(0,0,0,160));
  p.drawText(x+2, y+2, sadStr);
  p.drawText(x+2, y+2+lineH, srStr);
  //p.drawText(x+2, y+2+lineH, battStr);

  // основной белый текст
  if (menuState == sadChangeState)
    p.setPen(Qt::green);
  else if (menuState == normalState)
    p.setPen(QColor(50, 50, 50));
  else
    p.setPen(Qt::white);
  p.drawText(x, y, sadStr);

  if (menuState == srChangeState)  
    p.setPen(Qt::green);
  else if (menuState == normalState)
    p.setPen(QColor(50, 50, 50));
  else
    p.setPen(Qt::white);
  p.drawText(x, y+lineH, srStr);
}