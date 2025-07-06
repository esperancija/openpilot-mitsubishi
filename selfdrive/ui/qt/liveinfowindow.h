#pragma once
#include <QWidget>
#include "selfdrive/ui/ui.h"          // UIState

enum Key {noKey = 0, lkasOnKey, cancelKey, accOnKey, upKey, downKey};
enum State {normalState = 0, sadChangeState, srChangeState, lastState};

class LiveInfoWindow : public QWidget {
  Q_OBJECT
public:
  explicit LiveInfoWindow(QWidget *parent = nullptr);

public slots:
  void updateState(const UIState &s);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  int cpu  = -1;      // последние данные
  int batt = -1;
  int lastBtn = 0;
  int btnPressCnt = 0;
  float steerActuatorDelay = 0;
  float steerRatio = 0;
};