from collections import defaultdict
from enum import IntFlag

from cereal import car
from selfdrive.car import dbc_dict
from selfdrive.config import Conversions as CV

Ecu = car.CarParams.Ecu
MIN_ACC_SPEED = 40. * CV.KPH_TO_MS
PEDAL_TRANSITION = 10. * CV.MPH_TO_MS


class CarControllerParams:
  ACCEL_MAX = 1.5  # m/s2, lower than allowed 2.0 m/s2 for tuning reasons
  ACCEL_MIN = -3.5  # m/s2

  STEER_DRIVER_ALLOWANCE = 50
  STEER_DRIVER_MULTIPLIER = 1
  STEER_DRIVER_FACTOR = 100

  STEER_MOMENT_MAX = 1000
  STEER_MAX = 1500
  STEER_DELTA_UP = 10       # 1.5s time to peak torque
  STEER_DELTA_DOWN = 25     # always lower than 45 otherwise the Rav4 faults (Prius seems ok with 50)
  STEER_ERROR_MAX = 50 #350     # max delta between torque cmd and torque motor


class CAR:
  OUTLANDER_GT = "OUTLANDER_GT_2016"

FINGERPRINTS = {
  CAR.OUTLANDER_GT: [{
    # 8: 8, 257: 8, 337: 8, 340: 8, 389: 8, 512: 8, 520: 8, 528: 8, 530: 8, 532: 8, 533: 8, 536: 8, 546: 8, 553: 8, 554: 8, 555: 8, 566: 8, 597: 8,
    # 613: 8,644: 8, 720: 8, 753: 8,768: 8,776: 8,782: 8,786: 8,789: 8,805: 8,808: 8,816: 8,824: 8,839: 8,854: 8, 856: 8, 857: 8, 858: 8, 869: 8, 912: 8,
    # 922: 8, 1045: 8, 1048: 8, 1060: 8, 1061: 8, 1184: 8, 1312: 8, 1534: 8, 1544: 8, 1777: 8, 1786: 8, 1787: 8, 1789: 8, 1790: 8, 1791: 8, 1980: 8,
    # 2015: 8, 2024: 8, 2025: 8

    8: 8, 257: 1, 337: 8, 340: 8, 389: 8, 512: 8, 520: 8, 528: 8, 530: 8, 532: 8, 533: 8, 536: 8, 546: 8, 553: 8, 554: 8, 555: 8, 566: 8, 597: 8,
    613: 8, 644: 8, 720: 8, 753: 8, 768: 8, 776: 8, 782: 8, 786: 8, 789: 8, 805: 6, 808: 8, 816: 8, 824: 8, 839: 8, 854: 7, 856: 8, 857: 8, 858: 8,
    869: 8, 912: 8, 922: 8, 1045: 7, 1048: 8, 1060: 8, 1061: 8, 1184: 8, 1312: 8, 1534: 8, 1544: 8, 1777: 8, 1786: 8, 1787: 8, 1789: 8, 1790: 8,
    1791: 8, 1980: 8, 952: 8

    # 753: 8, 768: 8, 808: 8, 816: 8, 528: 8, 533: 8, 776: 8, 786: 8, 854: 7, 1184: 8, 856: 8, 857: 8, 858: 8, 536: 8, 613: 8, 1048: 8, 566: 8,
    # 789: 8, 512: 8, 520: 8, 532: 8, 530: 8, 553: 8, 597: 8, 1061: 8, 1045: 7, 1980: 8, 340: 8, 782: 8, 824: 8, 1060: 8, 554: 8, 912: 8, 869: 8,
    # 389: 8, 1312: 8, 555: 8, 1790: 8, 644: 8, 922: 8, 1786: 8, 546: 8, 1544: 8, 839: 8, 1791: 8, 1789: 8, 1787: 8, 1534: 8, 257: 1, 1777: 8,
    # 805: 6, 8: 8, 720: 8, 337: 8

  }]
}

STEER_THRESHOLD = 100

DBC = {
  CAR.OUTLANDER_GT: dbc_dict('mitsubishi_outlander_gt', None)
}
