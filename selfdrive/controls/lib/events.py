import os
from enum import IntEnum
from typing import Dict, Union, Callable, List, Optional

from cereal import log, car
import cereal.messaging as messaging
from common.conversions import Conversions as CV
from common.realtime import DT_CTRL
from selfdrive.locationd.calibrationd import MIN_SPEED_FILTER
from selfdrive.version import get_short_branch

AlertSize = log.ControlsState.AlertSize
AlertStatus = log.ControlsState.AlertStatus
VisualAlert = car.CarControl.HUDControl.VisualAlert
AudibleAlert = car.CarControl.HUDControl.AudibleAlert
EventName = car.CarEvent.EventName


# Alert priorities
class Priority(IntEnum):
  LOWEST = 0
  LOWER = 1
  LOW = 2
  MID = 3
  HIGH = 4
  HIGHEST = 5


# Event types
class ET:
  ENABLE = 'enable'
  PRE_ENABLE = 'preEnable'
  OVERRIDE = 'override'
  NO_ENTRY = 'noEntry'
  WARNING = 'warning'
  USER_DISABLE = 'userDisable'
  SOFT_DISABLE = 'softDisable'
  IMMEDIATE_DISABLE = 'immediateDisable'
  PERMANENT = 'permanent'


# get event name from enum
EVENT_NAME = {v: k for k, v in EventName.schema.enumerants.items()}


class Events:
  def __init__(self):
    self.events: List[int] = []
    self.static_events: List[int] = []
    self.events_prev = dict.fromkeys(EVENTS.keys(), 0)

  @property
  def names(self) -> List[int]:
    return self.events

  def __len__(self) -> int:
    return len(self.events)

  def add(self, event_name: int, static: bool=False) -> None:
    if static:
      self.static_events.append(event_name)
    self.events.append(event_name)

  def clear(self) -> None:
    self.events_prev = {k: (v + 1 if k in self.events else 0) for k, v in self.events_prev.items()}
    self.events = self.static_events.copy()

  def any(self, event_type: str) -> bool:
    return any(event_type in EVENTS.get(e, {}) for e in self.events)

  def create_alerts(self, event_types: List[str], callback_args=None):
    if callback_args is None:
      callback_args = []

    ret = []
    for e in self.events:
      types = EVENTS[e].keys()
      for et in event_types:
        if et in types:
          alert = EVENTS[e][et]
          if not isinstance(alert, Alert):
            alert = alert(*callback_args)

          if DT_CTRL * (self.events_prev[e] + 1) >= alert.creation_delay:
            alert.alert_type = f"{EVENT_NAME[e]}/{et}"
            alert.event_type = et
            ret.append(alert)
    return ret

  def add_from_msg(self, events):
    for e in events:
      self.events.append(e.name.raw)

  def to_msg(self):
    ret = []
    for event_name in self.events:
      event = car.CarEvent.new_message()
      event.name = event_name
      for event_type in EVENTS.get(event_name, {}):
        setattr(event, event_type, True)
      ret.append(event)
    return ret


class Alert:
  def __init__(self,
               alert_text_1: str,
               alert_text_2: str,
               alert_status: log.ControlsState.AlertStatus,
               alert_size: log.ControlsState.AlertSize,
               priority: Priority,
               visual_alert: car.CarControl.HUDControl.VisualAlert,
               audible_alert: car.CarControl.HUDControl.AudibleAlert,
               duration: float,
               alert_rate: float = 0.,
               creation_delay: float = 0.):

    self.alert_text_1 = alert_text_1
    self.alert_text_2 = alert_text_2
    self.alert_status = alert_status
    self.alert_size = alert_size
    self.priority = priority
    self.visual_alert = visual_alert
    self.audible_alert = audible_alert

    self.duration = int(duration / DT_CTRL)

    self.alert_rate = alert_rate
    self.creation_delay = creation_delay

    self.alert_type = ""
    self.event_type: Optional[str] = None

  def __str__(self) -> str:
    return f"{self.alert_text_1}/{self.alert_text_2} {self.priority} {self.visual_alert} {self.audible_alert}"

  def __gt__(self, alert2) -> bool:
    return self.priority > alert2.priority


class NoEntryAlert(Alert):
  def __init__(self, alert_text_2: str, visual_alert: car.CarControl.HUDControl.VisualAlert=VisualAlert.none):
    super().__init__("openpilot недоступен", alert_text_2, AlertStatus.normal,
                     AlertSize.mid, Priority.LOW, visual_alert,
                     AudibleAlert.refuse, 3.)


class SoftDisableAlert(Alert):
  def __init__(self, alert_text_2: str):
    super().__init__("ВОЗЬМИТЕ УПРАВЛЕНИЕ НА СЕБЯ!", alert_text_2,
                     AlertStatus.userPrompt, AlertSize.full,
                     Priority.MID, VisualAlert.steerRequired,
                     AudibleAlert.warningSoft, 2.),


# less harsh version of SoftDisable, where the condition is user-triggered
class UserSoftDisableAlert(SoftDisableAlert):
  def __init__(self, alert_text_2: str):
    super().__init__(alert_text_2),
    self.alert_text_1 = "openpilot отключится"


class ImmediateDisableAlert(Alert):
  def __init__(self, alert_text_2: str):
    super().__init__("ВОЗЬМИТЕ УПРАВЛЕНИЕ НА СЕБЯ!", alert_text_2,
                     AlertStatus.critical, AlertSize.full,
                     Priority.HIGHEST, VisualAlert.steerRequired,
                     AudibleAlert.warningImmediate, 4.),


class EngagementAlert(Alert):
  def __init__(self, audible_alert: car.CarControl.HUDControl.AudibleAlert):
    super().__init__("", "",
                     AlertStatus.normal, AlertSize.none,
                     Priority.MID, VisualAlert.none,
                     audible_alert, .2),


class NormalPermanentAlert(Alert):
  def __init__(self, alert_text_1: str, alert_text_2: str = "", duration: float = 0.2, priority: Priority = Priority.LOWER, creation_delay: float = 0.):
    super().__init__(alert_text_1, alert_text_2,
                     AlertStatus.normal, AlertSize.mid if len(alert_text_2) else AlertSize.small,
                     priority, VisualAlert.none, AudibleAlert.none, duration, creation_delay=creation_delay),


class StartupAlert(Alert):
  def __init__(self, alert_text_1: str, alert_text_2: str = "Всегда держите руки на руле и смотрите на дорогу", alert_status=AlertStatus.normal):
    super().__init__(alert_text_1, alert_text_2,
                     alert_status, AlertSize.mid,
                     Priority.LOWER, VisualAlert.none, AudibleAlert.none, 10.),


# ********** helper functions **********
def get_display_speed(speed_ms: float, metric: bool) -> str:
  speed = int(round(speed_ms * (CV.MS_TO_KPH if metric else CV.MS_TO_MPH)))
  unit = 'км/ч' if metric else 'миль/ч'
  return f"{speed} {unit}"


# ********** alert callback functions **********

AlertCallbackType = Callable[[car.CarParams, messaging.SubMaster, bool, int], Alert]


def soft_disable_alert(alert_text_2: str) -> AlertCallbackType:
  def func(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
    if soft_disable_time < int(0.5 / DT_CTRL):
      return ImmediateDisableAlert(alert_text_2)
    return SoftDisableAlert(alert_text_2)
  return func

def user_soft_disable_alert(alert_text_2: str) -> AlertCallbackType:
  def func(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
    if soft_disable_time < int(0.5 / DT_CTRL):
      return ImmediateDisableAlert(alert_text_2)
    return UserSoftDisableAlert(alert_text_2)
  return func

def startup_master_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  branch = get_short_branch("")
  if "REPLAY" in os.environ:
    branch = "replay"

  return StartupAlert("ВНИМАНИЕ! Эта ветка тестируется!", branch, alert_status=AlertStatus.userPrompt)

def below_engage_speed_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  return NoEntryAlert(f"Speed Below {get_display_speed(CP.minEnableSpeed, metric)}")


def below_steer_speed_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  return Alert(
    f"Steer Unavailable Below {get_display_speed(CP.minSteerSpeed, metric)}",
    "",
    AlertStatus.userPrompt, AlertSize.small,
    Priority.MID, VisualAlert.steerRequired, AudibleAlert.prompt, 0.4)


def calibration_incomplete_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  return Alert(
    "Calibration in Progress: %d%%" % sm['liveCalibration'].calPerc,
    f"Drive Above {get_display_speed(MIN_SPEED_FILTER, metric)}",
    AlertStatus.normal, AlertSize.mid,
    Priority.LOWEST, VisualAlert.none, AudibleAlert.none, .2)


def no_gps_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  gps_integrated = sm['peripheralState'].pandaType in (log.PandaState.PandaType.uno, log.PandaState.PandaType.dos)
  return Alert(
    "Плохой прием GPS",
    "Неисправность оборудования" if gps_integrated else "Проверьте расположение антенны GPS",
    AlertStatus.normal, AlertSize.mid,
    Priority.LOWER, VisualAlert.none, AudibleAlert.none, .2, creation_delay=300.)


def wrong_car_mode_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  text = "Круиз-контроль отключен"
  if CP.carName == "honda":
    text = "Main Switch Off"
  return NoEntryAlert(text)


def joystick_alert(CP: car.CarParams, sm: messaging.SubMaster, metric: bool, soft_disable_time: int) -> Alert:
  axes = sm['testJoystick'].axes
  gb, steer = list(axes)[:2] if len(axes) else (0., 0.)
  vals = f"Gas: {round(gb * 100.)}%, Steer: {round(steer * 100.)}%"
  return NormalPermanentAlert("Joystick Mode", vals)



EVENTS: Dict[int, Dict[str, Union[Alert, AlertCallbackType]]] = {
  # ********** events with no alerts **********

  EventName.stockFcw: {},

  # ********** events only containing alerts displayed in all states **********

  EventName.joystickDebug: {
    ET.WARNING: joystick_alert,
    ET.PERMANENT: NormalPermanentAlert("Режим джойстика"),
  },

  EventName.controlsInitializing: {
    ET.NO_ENTRY: NoEntryAlert("Инициализация системы"),
  },

  EventName.startup: {
    ET.PERMANENT: StartupAlert("Будьте готовы взять управление на себя в любой момент")
  },

  EventName.startupMaster: {
    ET.PERMANENT: startup_master_alert,
  },

  # Car is recognized, but marked as dashcam only
  EventName.startupNoControl: {
    ET.PERMANENT: StartupAlert("Режим видеорегистратора"),
  },

  # Car is not recognized
  EventName.startupNoCar: {
    ET.PERMANENT: StartupAlert("Режим видеорегистратора для неподдерживаемого автомобиля"),
  },

  EventName.startupNoFw: {
    ET.PERMANENT: StartupAlert("Автомобиль не опознан",
                               "Проверьте подключение питания comma",
                               alert_status=AlertStatus.userPrompt),
  },

  EventName.dashcamMode: {
    ET.PERMANENT: NormalPermanentAlert("Режим видеорегистратора",
                                       priority=Priority.LOWEST),
  },

  EventName.invalidLkasSetting: {
    ET.PERMANENT: NormalPermanentAlert("Stock LKAS is on",
                                       "Turn off stock LKAS to engage"),
  },

  EventName.cruiseMismatch: {
    #ET.PERMANENT: ImmediateDisableAlert("openpilot не удалось отключить круиз-контроль"),
  },

  # openpilot doesn't recognize the car. This switches openpilot into a
  # read-only mode. This can be solved by adding your fingerprint.
  # See https://github.com/commaai/openpilot/wiki/Fingerprinting for more information
  EventName.carUnrecognized: {
    ET.PERMANENT: NormalPermanentAlert("Режим видеорегистратора",
                                       "Автомобиль не опознан",
                                       priority=Priority.LOWEST),
  },

  EventName.stockAeb: {
    ET.PERMANENT: Alert(
      "ТОРМОЗИТЕ!",
      "Заводской AEB: РИСК СТОЛКНОВЕНИЯ",
      AlertStatus.critical, AlertSize.full,
      Priority.HIGHEST, VisualAlert.fcw, AudibleAlert.none, 2.),
    ET.NO_ENTRY: NoEntryAlert("Stock AEB: Risk of Collision"),
  },

  EventName.fcw: {
    ET.PERMANENT: Alert(
      "ТОРМОЗИТЕ!",
      "РИСК СТОЛКНОВЕНИЯ",
      AlertStatus.critical, AlertSize.full,
      Priority.HIGHEST, VisualAlert.fcw, AudibleAlert.warningSoft, 2.),
  },

  EventName.ldw: {
    ET.PERMANENT: Alert(
      "Выезд с полосы движения!",
      "",
      AlertStatus.userPrompt, AlertSize.small,
      Priority.LOW, VisualAlert.ldw, AudibleAlert.prompt, 3.),
  },

  # ********** events only containing alerts that display while engaged **********

  # openpilot tries to learn certain parameters about your car by observing
  # how the car behaves to steering inputs from both human and openpilot driving.
  # This includes:
  # - steer ratio: gear ratio of the steering rack. Steering angle divided by tire angle
  # - tire stiffness: how much grip your tires have
  # - angle offset: most steering angle sensors are offset and measure a non zero angle when driving straight
  # This alert is thrown when any of these values exceed a sanity check. This can be caused by
  # bad alignment or bad sensor data. If this happens consistently consider creating an issue on GitHub
  EventName.vehicleModelInvalid: {
    ET.NO_ENTRY: NoEntryAlert("Не удалось идентифицировать параметры ТС"),
    ET.SOFT_DISABLE: soft_disable_alert("Не удалось идентифицировать параметры ТС"),
  },

  EventName.steerTempUnavailableSilent: {
    ET.WARNING: Alert(
      "Рулевое управление временно недоступно",
      "",
      AlertStatus.userPrompt, AlertSize.small,
      Priority.LOW, VisualAlert.steerRequired, AudibleAlert.prompt, 1.),
  },

  EventName.preDriverDistracted: {
    ET.WARNING: Alert(
      "Обратите внимание!",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .1),
  },

  EventName.promptDriverDistracted: {
    ET.WARNING: Alert(
      "Обратите внимание!",
      "Водитель отвлекся!",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.MID, VisualAlert.steerRequired, AudibleAlert.promptDistracted, .1),
  },

  EventName.driverDistracted: {
    ET.WARNING: Alert(
      "НЕМЕДЛЕННО ОТКЛЮЧИТЕ OP",
      "Водитель отвлекся!",
      AlertStatus.critical, AlertSize.full,
      Priority.HIGH, VisualAlert.steerRequired, AudibleAlert.warningImmediate, .1),
  },

  EventName.preDriverUnresponsive: {
    ET.WARNING: Alert(
      "Держитесь за руль! Лицо водителя не обнаружено",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.steerRequired, AudibleAlert.none, .1, alert_rate=0.75),
  },

  EventName.promptDriverUnresponsive: {
    ET.WARNING: Alert(
      "Держитесь за руль!",
      "Водитель не реагирует!",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.MID, VisualAlert.steerRequired, AudibleAlert.promptDistracted, .1),
  },

  EventName.driverUnresponsive: {
    ET.WARNING: Alert(
      "НЕМЕДЛЕННО ОТКЛЮЧИТЕ OP",
      "Водитель не реагирует!",
      AlertStatus.critical, AlertSize.full,
      Priority.HIGH, VisualAlert.steerRequired, AudibleAlert.warningImmediate, .1),
  },

  EventName.manualRestart: {
    ET.WARNING: Alert(
      "ВОЗЬМИТЕ УПРАВЛЕНИЕ ПОД КОНТРОЛЬ!",
      "Возобновите ручное управление",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .2),
  },

  EventName.resumeRequired: {
    ET.WARNING: Alert(
      "ОСТАНОВИТЕСЬ",
      "Нажмите ВОЗВРАТ для движения",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .2),
  },

  EventName.belowSteerSpeed: {
    ET.WARNING: below_steer_speed_alert,
  },

  EventName.preLaneChangeLeft: {
    ET.WARNING: Alert(
      "Поверните налево, чтобы начать перестроение в безопасную полосу движения",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .1, alert_rate=0.75),
  },

  EventName.preLaneChangeRight: {
    ET.WARNING: Alert(
      "Поверните направо, чтобы начать перестроение в безопасную полосу движения",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .1, alert_rate=0.75),
  },

  EventName.laneChangeBlocked: {
    ET.WARNING: Alert(
      "В слепой зоне обнаружен автомобиль",
      "",
      AlertStatus.userPrompt, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.prompt, .1),
  },

  EventName.laneChange: {
    ET.WARNING: Alert(
      "Смена полосы движения",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, .1),
  },

  EventName.steerSaturated: {
    ET.WARNING: Alert(
      "ВОзьмите управление на себя!",
      "Поворот превышает ограничение рулевого управления",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.LOW, VisualAlert.steerRequired, AudibleAlert.promptRepeat, 1.),
  },

  # Thrown when the fan is driven at >50% but is not rotating
  EventName.fanMalfunction: {
    ET.PERMANENT: NormalPermanentAlert("Неисправность вентилятора", "Проблема с оборудованием"),
  },

  # Camera is not outputting frames at a constant framerate
  EventName.cameraMalfunction: {
    ET.PERMANENT: NormalPermanentAlert("Неисправность камеры", "Проблема с оборудованием"),
  },

  # Unused
  EventName.gpsMalfunction: {
    ET.PERMANENT: NormalPermanentAlert("Неисправность GPS", "Проблема с оборудованием"),
  },

  # When the GPS position and localizer diverge the localizer is reset to the
  # current GPS position. This alert is thrown when the localizer is reset
  # more often than expected.
  EventName.localizerMalfunction: {
    # ET.PERMANENT: NormalPermanentAlert("Неисправность датчика", "Аппаратная неисправность"),
  },

  # ********** events that affect controls state transitions **********

  EventName.pcmEnable: {
    ET.ENABLE: EngagementAlert(AudibleAlert.engage),
  },

  EventName.buttonEnable: {
    ET.ENABLE: EngagementAlert(AudibleAlert.engage),
  },

  EventName.pcmDisable: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
  },

  EventName.buttonCancel: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
  },

  EventName.brakeHold: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
    ET.NO_ENTRY: NoEntryAlert("Активировано удержание тормоза"),
  },

  EventName.parkBrake: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
    ET.NO_ENTRY: NoEntryAlert("Включен стояночный тормоз"),
  },

  EventName.pedalPressed: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
    ET.NO_ENTRY: NoEntryAlert("Нажата педаль",
                              visual_alert=VisualAlert.brakePressed),
  },

  EventName.pedalPressedPreEnable: {
    ET.PRE_ENABLE: Alert(
      "Отпустите педаль для включения",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOWEST, VisualAlert.none, AudibleAlert.none, .1, creation_delay=1.),
  },

  EventName.gasPressedOverride: {
    ET.OVERRIDE: Alert(
      "",
      "",
      AlertStatus.normal, AlertSize.none,
      Priority.LOWEST, VisualAlert.none, AudibleAlert.none, .1),
  },

  EventName.wrongCarMode: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
    ET.NO_ENTRY: wrong_car_mode_alert,
  },

  EventName.wrongCruiseMode: {
    ET.USER_DISABLE: EngagementAlert(AudibleAlert.disengage),
    ET.NO_ENTRY: NoEntryAlert("Адаптивный круиз отключен"),
  },

  EventName.steerTempUnavailable: {
    ET.SOFT_DISABLE: soft_disable_alert("Управление рулем недоступно"),
    ET.NO_ENTRY: NoEntryAlert("Управление рулем недоступно"),
  },

  EventName.outOfSpace: {
    ET.PERMANENT: NormalPermanentAlert("Выезд за пределы"),
    ET.NO_ENTRY: NoEntryAlert("Выезд за пределы"),
  },

  EventName.belowEngageSpeed: {
    ET.NO_ENTRY: below_engage_speed_alert,
  },

  EventName.sensorDataInvalid: {
    ET.PERMANENT: Alert(
      "Нет данных с датчиков устройства",
      "Перезагрузите устройство",
      AlertStatus.normal, AlertSize.mid,
      Priority.LOWER, VisualAlert.none, AudibleAlert.none, .2, creation_delay=1.),
    ET.NO_ENTRY: NoEntryAlert("Нет данных с датчиков устройства"),
  },

  EventName.noGps: {
    #ET.PERMANENT: no_gps_alert,
  },

  EventName.soundsUnavailable: {
    ET.PERMANENT: NormalPermanentAlert("Динамик не найден", "Перезагрузите устройство"),
    ET.NO_ENTRY: NoEntryAlert("Динамик не найден"),
  },

  EventName.tooDistracted: {
    ET.NO_ENTRY: NoEntryAlert("Много отвлекаетесь"),
  },

  EventName.overheat: {
    ET.PERMANENT: NormalPermanentAlert("Перегрев"),
    ET.SOFT_DISABLE: soft_disable_alert("Перегрев"),
    ET.NO_ENTRY: NoEntryAlert("Перегрев"),
  },

  EventName.wrongGear: {
    ET.SOFT_DISABLE: user_soft_disable_alert("Не включена передача D"),
    ET.NO_ENTRY: NoEntryAlert("Не включена передача D"),
  },

  # This alert is thrown when the calibration angles are outside of the acceptable range.
  # For example if the device is pointed too much to the left or the right.
  # Usually this can only be solved by removing the mount from the windshield completely,
  # and attaching while making sure the device is pointed straight forward and is level.
  # See https://comma.ai/setup for more information
  EventName.calibrationInvalid: {
    ET.PERMANENT: NormalPermanentAlert("Calibration Invalid", "Remount Device and Recalibrate"),
    ET.SOFT_DISABLE: soft_disable_alert("Calibration Invalid: Remount Device & Recalibrate"),
    ET.NO_ENTRY: NoEntryAlert("Calibration Invalid: Remount Device & Recalibrate"),
  },

  EventName.calibrationIncomplete: {
    ET.PERMANENT: calibration_incomplete_alert,
    ET.SOFT_DISABLE: soft_disable_alert("Выполняется калибровка"),
    ET.NO_ENTRY: NoEntryAlert("Выполняется калибровка"),
  },

  EventName.doorOpen: {
    ET.SOFT_DISABLE: user_soft_disable_alert("Закройте дверь"),
    ET.NO_ENTRY: NoEntryAlert("Закройте дверь"),
  },

  EventName.seatbeltNotLatched: {
    ET.SOFT_DISABLE: user_soft_disable_alert("Пристегните ремень безопасности"),
    ET.NO_ENTRY: NoEntryAlert("Пристегните ремень безопасности"),
  },

  EventName.espDisabled: {
    ET.SOFT_DISABLE: soft_disable_alert("ESP отключено"),
    ET.NO_ENTRY: NoEntryAlert("ESP отключено"),
  },

  EventName.lowBattery: {
    ET.SOFT_DISABLE: soft_disable_alert("Низкий заряд батареи"),
    ET.NO_ENTRY: NoEntryAlert("Низкий заряд батареи"),
  },

  # Different openpilot services communicate between each other at a certain
  # interval. If communication does not follow the regular schedule this alert
  # is thrown. This can mean a service crashed, did not broadcast a message for
  # ten times the regular interval, or the average interval is more than 10% too high.
  EventName.commIssue: {
    ET.SOFT_DISABLE: soft_disable_alert("Проблема связи между устройствами"),
    ET.NO_ENTRY: NoEntryAlert("Проблема связи между устройствами"),
  },

  # Thrown when manager detects a service exited unexpectedly while driving
  EventName.processNotRunning: {
    ET.NO_ENTRY: NoEntryAlert("Системный сбой: Перезагрузите устройство"),
  },

  EventName.radarFault: {
    ET.SOFT_DISABLE: soft_disable_alert("Ошибка радара: Перезапустите автомобиль"),
    ET.NO_ENTRY: NoEntryAlert("Ошибка радара: Перезапустите автомобиль"),
  },

  # Every frame from the camera should be processed by the model. If modeld
  # is not processing frames fast enough they have to be dropped. This alert is
  # thrown when over 20% of frames are dropped.
  EventName.modeldLagging: {
    ET.SOFT_DISABLE: soft_disable_alert("Задержки при моделировании"),
    ET.NO_ENTRY: NoEntryAlert("Задержки при моделировании"),
  },

  # Besides predicting the path, lane lines and lead car data the model also
  # predicts the current velocity and rotation speed of the car. If the model is
  # very uncertain about the current velocity while the car is moving, this
  # usually means the model has trouble understanding the scene. This is used
  # as a heuristic to warn the driver.
  EventName.posenetInvalid: {
    ET.SOFT_DISABLE: soft_disable_alert("Моделирование не точно"),
    ET.NO_ENTRY: NoEntryAlert("Моделирование не точно"),
  },

  # When the localizer detects an acceleration of more than 40 m/s^2 (~4G) we
  # alert the driver the device might have fallen from the windshield.
  EventName.deviceFalling: {
    ET.SOFT_DISABLE: soft_disable_alert("Устройство сорвалось с Крепления"),
    ET.NO_ENTRY: NoEntryAlert("Устройство сорвалось с Крепления"),
  },

  EventName.lowMemory: {
    ET.SOFT_DISABLE: soft_disable_alert("Недостаточно памяти: Перезагрузите устройство"),
    ET.PERMANENT: NormalPermanentAlert("Недостаточно памяти", "Перезагрузите устройство"),
    ET.NO_ENTRY: NoEntryAlert("Недостаточно памяти: Перезагрузите устройство"),
  },

  EventName.highCpuUsage: {
    #ET.SOFT_DISABLE: soft_disable_alert("Системный сбой: Перезагрузите устройство"),
    #ET.PERMANENT: NormalPermanentAlert("Системный сбой", "Перезагрузите устройство"),
    ET.NO_ENTRY: NoEntryAlert("Системный сбой: Перезагрузите устройство"),
  },

  EventName.accFaulted: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Круиз-контроль неисправен"),
    ET.PERMANENT: NormalPermanentAlert("Круиз-контроль неисправен", ""),
    ET.NO_ENTRY: NoEntryAlert("Круиз-контроль неисправен"),
  },

  EventName.controlsMismatch: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Несоответствие элементов управления"),
  },

  EventName.roadCameraError: {
    ET.PERMANENT: NormalPermanentAlert("Ошибка камеры",
                                       duration=1.,
                                       creation_delay=30.),
  },

  EventName.driverCameraError: {
    ET.PERMANENT: NormalPermanentAlert("Ошибка камеры",
                                       duration=1.,
                                       creation_delay=30.),
  },

  EventName.wideRoadCameraError: {
    ET.PERMANENT: NormalPermanentAlert("Ошибка камеры",
                                       duration=1.,
                                       creation_delay=30.),
  },

  # Sometimes the USB stack on the device can get into a bad state
  # causing the connection to the panda to be lost
  EventName.usbError: {
    ET.SOFT_DISABLE: soft_disable_alert("Ошибка USB: Перезагрузите устройство"),
    ET.PERMANENT: NormalPermanentAlert("Ошибка USB: Перезагрузите устройство", ""),
    ET.NO_ENTRY: NoEntryAlert("Ошибка USB: Перезагрузите устройство"),
  },

  # This alert can be thrown for the following reasons:
  # - No CAN data received at all
  # - CAN data is received, but some message are not received at the right frequency
  # If you're not writing a new car port, this is usually cause by faulty wiring
  EventName.canError: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Ошибка CAN: Проверьте соединения"),
    ET.PERMANENT: Alert(
      "Ошибка CAN: Проверьте соединения",
      "",
      AlertStatus.normal, AlertSize.small,
      Priority.LOW, VisualAlert.none, AudibleAlert.none, 1., creation_delay=1.),
    ET.NO_ENTRY: NoEntryAlert("Ошибка CAN: Проверьте соединения"),
  },

  EventName.steerUnavailable: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("LKAS Fault: Restart the Car"),
    ET.PERMANENT: NormalPermanentAlert("LKAS Fault: Restart the car to engage"),
    ET.NO_ENTRY: NoEntryAlert("LKAS Fault: Restart the Car"),
  },

  EventName.brakeUnavailable: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Круиз-контроль неисправен: Перезапустите автомобиль"),
    ET.PERMANENT: NormalPermanentAlert("Круиз-контроль неисправен: Перезапустите автомобиль"),
    ET.NO_ENTRY: NoEntryAlert("Круиз-контроль неисправен: Перезапустите автомобиль"),
  },

  EventName.reverseGear: {
    ET.PERMANENT: Alert(
      "Reverse\nGear",
      "",
      AlertStatus.normal, AlertSize.full,
      Priority.LOWEST, VisualAlert.none, AudibleAlert.none, .2, creation_delay=0.5),
    ET.USER_DISABLE: ImmediateDisableAlert("Задний ход"),
    ET.NO_ENTRY: NoEntryAlert("Задний ход"),
  },

  # On cars that use stock ACC the car can decide to cancel ACC for various reasons.
  # When this happens we can no long control the car so the user needs to be warned immediately.
  EventName.cruiseDisabled: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Круиз-контроль выключен"),
  },

  # For planning the trajectory Model Predictive Control (MPC) is used. This is
  # an optimization algorithm that is not guaranteed to find a feasible solution.
  # If no solution is found or the solution has a very high cost this alert is thrown.
  EventName.plannerError: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Ошибка планировщика"),
    ET.NO_ENTRY: NoEntryAlert("Ошибка планировщика"),
  },

  # When the relay in the harness box opens the CAN bus between the LKAS camera
  # and the rest of the car is separated. When messages from the LKAS camera
  # are received on the car side this usually means the relay hasn't opened correctly
  # and this alert is thrown.
  EventName.relayMalfunction: {
    ET.IMMEDIATE_DISABLE: ImmediateDisableAlert("Неисправность жгута проводов"),
    ET.PERMANENT: NormalPermanentAlert("Неисправность жгута проводов", "Проверьте оборудование"),
    ET.NO_ENTRY: NoEntryAlert("Неисправность жгута проводов"),
  },

  EventName.noTarget: {
    ET.IMMEDIATE_DISABLE: Alert(
      "openpilot отключен",
      "No close lead car",
      AlertStatus.normal, AlertSize.mid,
      Priority.HIGH, VisualAlert.none, AudibleAlert.disengage, 3.),
    ET.NO_ENTRY: NoEntryAlert("No Close Lead Car"),
  },

  EventName.speedTooLow: {
    ET.IMMEDIATE_DISABLE: Alert(
      "openpilot отключен",
      "Слишком низкая скорость",
      AlertStatus.normal, AlertSize.mid,
      Priority.HIGH, VisualAlert.none, AudibleAlert.disengage, 3.),
  },

  # When the car is driving faster than most cars in the training data, the model outputs can be unpredictable.
  EventName.speedTooHigh: {
    ET.WARNING: Alert(
      "Слишком высокая скорость",
      "Моделирование недоступно на такой скорости",
      AlertStatus.userPrompt, AlertSize.mid,
      Priority.HIGH, VisualAlert.steerRequired, AudibleAlert.promptRepeat, 4.),
    ET.NO_ENTRY: NoEntryAlert("Сбавьте скорость"),
  },

  EventName.lowSpeedLockout: {
    ET.PERMANENT: NormalPermanentAlert("Неисправность круиз-контроля: Перезапустите автомобиль, чтобы включить"),
    ET.NO_ENTRY: NoEntryAlert("Неисправность круиз-контроля: Перезапустите автомобиль"),
  },

  EventName.lkasDisabled: {
    ET.PERMANENT: NormalPermanentAlert("LKAS Disabled: Enable LKAS to engage"),
    ET.NO_ENTRY: NoEntryAlert("LKAS Disabled"),
  },

}
