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
  lastBtn = gm.getPressedButton(); 
  btnPressCnt = gm.getBtnPressCnt();

  const auto lp = (*s.sm)["liveParameters"].getLiveParameters();
    steerRatio = lp.getSteerRatio();

  const auto cp = (*s.sm)["carParams"].getCarParams();
    steerActuatorDelay = cp.getSteerActuatorDelay();

  

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
  
  static int state = 0, oldButton = 0;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(QFont("Inter", 10, QFont::Bold));

  // строки
  QString sadStr  = QString("delay  : %1").arg(steerActuatorDelay);
  QString srStr  = QString("ratio  : %1").arg(steerRatio);

  // координаты (правый-верхний угол)
  const int margin = 40;//32;
  const int lineH  = p.fontMetrics().height() + 10;
  int x = width()  - p.fontMetrics().horizontalAdvance(sadStr) - margin;
  int y = margin + lineH;

  // if ((oldButton != lastBtn)){
  //   switch (oldButton){
  //     case cancelKey:
  //       if (((state == normalState) && (btnPressCnt > 24)) || 
  //           ((state == sadChangeState) && (btnPressCnt > 0)) || 
  //              ((state == srChangeState) && (btnPressCnt > 0))){
  //         state++;
  //       }
  //     case lkasOnKey:
  //     case accOnKey:
  //       state = normalState;
  //       break;
  //     case upKey:
  //       if (state == sadChangeState){

  //       }
  //       break;
  //     case downKey:
  //        if (state == srChangeState){
          
  //       }
  //       break;
  //     default:
  //   }
  // }
  // if (state >= lastState )
  //   state = sadChangeState;

  // лёгкая тень
  p.setPen(QColor(0,0,0,160));
  p.drawText(x+2, y+2, sadStr);
  p.drawText(x+2, y+2+lineH, srStr);
  //p.drawText(x+2, y+2+lineH, battStr);

  // основной белый текст
  p.setPen(Qt::white);
  if ((lastBtn == cancelKey) && (btnPressCnt > 24))
    p.setPen(Qt::green);
  else if (state == sadChangeState)
    p.setPen(Qt::red);
  else if (state == srChangeState)  
    p.setPen(Qt::blue);
  p.drawText(x, y, sadStr);
  p.drawText(x, y+lineH, srStr);
  // p.drawText(x, y+lineH, battStr);

  oldButton = lastBtn;
}