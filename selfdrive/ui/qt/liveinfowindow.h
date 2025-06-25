#pragma once
#include <QWidget>
#include "selfdrive/ui/ui.h"          // UIState

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
};