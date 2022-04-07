from cereal import car
from common.numpy_fast import mean
from common.filter_simple import FirstOrderFilter
from common.realtime import DT_CTRL
from opendbc.can.can_define import CANDefine
from selfdrive.car.interfaces import CarStateBase
from opendbc.can.parser import CANParser
from selfdrive.config import Conversions as CV
from selfdrive.car.mitsubishi.values import DBC


class CarState(CarStateBase):
  def __init__(self, CP):
    super().__init__(CP)

    can_define = CANDefine(DBC[CP.carFingerprint]["pt"])
    self.shifter_values = can_define.dv["GEARBOX"]["GEAR_SHIFTER"]

    # On cars with cp.vl["STEER_TORQUE_SENSOR"]["STEER_ANGLE"]
    # the signal is zeroed to where the steering angle is at start.
    # Need to apply an offset as soon as the steering angle measurements are both received
    self.needs_angle_offset = True
    self.accurate_steer_angle_seen = False
    self.angle_offset = 0

    self.low_speed_lockout = False
    self.acc_type = 1

  def swapBytesSigned(self, data):
    ret = ((data & 0xff) << 8) + ((data >> 8)  & 0xff)
    if (ret > 32768):
      ret = ret - 65536
    return ret

  def swapBytesUnsigned(self, data):
    return ((data & 0xff) << 8) + ((data >> 8)  & 0xff)

  def update(self, cp, cp_cam):
    ret = car.CarState.new_message()

    ret.doorOpen = any([cp.vl["DOORS_STATUS"]["DOOR_OPEN_FL"], cp.vl["DOORS_STATUS"]["DOOR_OPEN_FR"],
                        cp.vl["DOORS_STATUS"]["DOOR_OPEN_RL"], cp.vl["DOORS_STATUS"]["DOOR_OPEN_RR"]])
    ret.seatbeltUnlatched = cp.vl["SEATBELT_STATUS"]["SEATBELT_DRIVER_UNLATCHED"] != 0

    ret.brakePressed = cp.vl["BRAKE_MODULE"]["BRAKE_PRESSED"] != 0

    #ret.gas = cp.vl["GAS_PEDAL"]["GAS_PEDAL"]
    #ret.gasPressed = ret.gas > 2

    #print("gas = %d" % (ret.gas))
    ret.gas = cp.vl["JOYSTICK_COMMAND"]["TEST_DATA"]
    ret.gasPressed = False

    # erpm = int(cp.vl["ENGINE_RPM_ID"]["ENGINE_RPM"])
    # erpm = ((erpm & 0xff) << 8) + ((erpm >> 8)  & 0xff)
    #ret.engineRPM  = self.swapBytesUnsigned(int(cp.vl["ENGINE_RPM_ID"]["ENGINE_RPM"])) #cp.vl["ENGINE_RPM_ID"]["ENGINE_RPM"]


    speed_factor = 0.3/4
    ret.wheelSpeeds.fl = self.swapBytesUnsigned(int(cp.vl["WHEEL_SPEEDS_1"]["WHEEL_SPEED_FL"])) * CV.KPH_TO_MS * speed_factor
    ret.wheelSpeeds.fr = self.swapBytesUnsigned(int(cp.vl["WHEEL_SPEEDS_1"]["WHEEL_SPEED_FR"])) * CV.KPH_TO_MS * speed_factor
    ret.wheelSpeeds.rl = self.swapBytesUnsigned(int(cp.vl["WHEEL_SPEEDS_1"]["WHEEL_SPEED_RL"])) * CV.KPH_TO_MS * speed_factor
    ret.wheelSpeeds.rr = self.swapBytesUnsigned(int(cp.vl["WHEEL_SPEEDS_2"]["WHEEL_SPEED_RR"])) * CV.KPH_TO_MS * speed_factor

    ret.wheelSpeeds.fl = cp.vl["GAS_PEDAL"]["GAS_PEDAL"]
    ret.wheelSpeeds.fr =  cp.vl["GAS_PEDAL"]["GAS_PEDAL"]
    ret.wheelSpeeds.rl = cp.vl["GAS_PEDAL"]["GAS_PEDAL"]
    ret.wheelSpeeds.rr = cp.vl["GAS_PEDAL"]["GAS_PEDAL"]


    ret.vEgoRaw = mean([ret.wheelSpeeds.fl, ret.wheelSpeeds.fr, ret.wheelSpeeds.rl, ret.wheelSpeeds.rr])
    ret.vEgo, ret.aEgo = self.update_speed_kf(ret.vEgoRaw)

    ret.standstill = ret.vEgoRaw < 10

    sta = self.swapBytesSigned(int(cp.vl["STEER_ANGLE_SENSOR"]["STEER_ANGLE"]))
    sta-=4096
    sta /= 2
    ret.steeringAngleDeg = sta

    ret.steeringRateDeg = self.swapBytesSigned(int(cp.vl["STEER_ANGLE_SENSOR"]["STEER_RATE"]))

    can_gear = int(cp.vl["GEARBOX"]["GEAR_SHIFTER"])
    ret.gearShifter = self.parse_gear_shifter(self.shifter_values.get(can_gear, None))

    # continuous blinker signals for assisted lane change
    ret.leftBlinker, ret.rightBlinker = self.update_blinker_from_lamp(
      50, cp.vl["WARNING_SIGNALS"]["TURN_LEFT_SIGNAL"], cp.vl["WARNING_SIGNALS"]["TURN_RIGHT_SIGNAL"])

    ret.steeringTorque = 0 #cp.vl["STEER_MOMENT_SENSOR"]["STEER_MOMENT"]
    ret.steeringTorqueEps = 0 #cp.vl["STEER_MOMENT_SENSOR"]["STEER_MOMENT_EPS"]

    #print("trq=%d, trqeps=%d" % (ret.steeringTorque, ret.steeringTorqueEps))

    # we could use the override bit from dbc, but it's triggered at too high torque values
    ret.steeringPressed = abs(ret.steeringTorque) > 1
    #ret.steerWarning = False#0

    #ret.cruiseState.available = cp.vl["ACC_STATUS"]["CRUISE_ON"] != 0
    ret.cruiseState.speed = cp.vl["ACC_STATUS"]["SET_SPEED"] * CV.KPH_TO_MS
    #ret.cruiseState.enabled = bool(cp.vl["ACC_STATUS"]["CRUISE_ACTIVE"])


    ret.cruiseState.enabled = bool(cp.vl["JOYSTICK_COMMAND"]["OP_ON"])
    ret.cruiseState.available = bool(cp.vl["JOYSTICK_COMMAND"]["OP_ON"])
    #print ("cruise %d %d" % (ret.cruiseState.enabled, ret.cruiseState.available))


    ret.cruiseState.nonAdaptive = False#cp.vl["ACC_STATUS"]["CRUISE_STATE"] in (1, 2, 3, 4, 5, 6)
    #ret.cruiseActualEnabled = ret.cruiseState.enabled

    ret.cruiseState.standstill = False

    #ret.stockAeb = bool(cp_cam.vl["PRE_COLLISION"]["PRECOLLISION_ACTIVE"] and cp_cam.vl["PRE_COLLISION"]["FORCE"] < -1e-5)
    #ret.espDisabled = cp.vl["ESP_CONTROL"]["TC_DISABLED"] != 0
    ret.leftBlindspot = bool(cp.vl["BSW_STATUS"]["LEFT_WARNING"])
    ret.rightBlindspot = bool(cp.vl["BSW_STATUS"]["RIGHT_WARNING"])

    return ret

  @staticmethod
  def get_can_parser(CP):
    signals = [
      # sig_name, sig_address
      ("DOOR_OPEN_FL", "DOORS_STATUS", 0),
      ("DOOR_OPEN_FR", "DOORS_STATUS", 0),
      ("DOOR_OPEN_RL", "DOORS_STATUS", 0),
      ("DOOR_OPEN_RR", "DOORS_STATUS", 0),
      ("SEATBELT_DRIVER_UNLATCHED", "SEATBELT_STATUS", 0),
      ("BRAKE_PRESSED", "BRAKE_MODULE", 0),
      ("GAS_PEDAL", "GAS_PEDAL", 0),
      ("ENGINE_RPM", "ENGINE_RPM_ID", 0),

      ("WHEEL_SPEED_FL", "WHEEL_SPEEDS_1", 0),
      ("WHEEL_SPEED_FR", "WHEEL_SPEEDS_1", 0),
      ("WHEEL_SPEED_RL", "WHEEL_SPEEDS_1", 0),
      ("WHEEL_SPEED_RR", "WHEEL_SPEEDS_2", 0),
      ("STEER_ANGLE", "STEER_ANGLE_SENSOR", 0),
      ("STEER_RATE", "STEER_ANGLE_SENSOR", 0),
      #("STEER_ANGLE_L", "STEER_ANGLE_SENSOR", 0),

      ("ECO_MODE", "ECO_BUT_ID", 0),

      ("GEAR_SHIFTER", "GEARBOX", 0),
      ("TURN_LEFT_SIGNAL", "WARNING_SIGNALS", 0),
      ("TURN_RIGHT_SIGNAL", "WARNING_SIGNALS", 0),
      ("STEER_MOMENT", "STEER_MOMENT_SENSOR", 0),
      ("STEER_MOMENT_EPS", "STEER_MOMENT_SENSOR", 0),
      ("CRUISE_ON", "ACC_STATUS", 0),
      ("CRUISE_ACTIVE", "ACC_STATUS", 0),
      ("SET_SPEED", "ACC_STATUS", 0),
      ("LEFT_WARNING", "BSW_STATUS", 0),
      ("RIGHT_WARNING", "BSW_STATUS", 0),

      ("TEST_DATA", "JOYSTICK_COMMAND", 0),
      ("OP_ON", "JOYSTICK_COMMAND", 0),
    ]

    checks = [
      ("DOORS_STATUS", 1),
      ("SEATBELT_STATUS", 1),
      ("BRAKE_MODULE", 1),
      ("GAS_PEDAL", 1),
      ("ENGINE_RPM_ID", 1),
      ("WHEEL_SPEEDS_1", 1),
      ("WHEEL_SPEEDS_2", 1),
      ("STEER_ANGLE_SENSOR", 1),
      ("ECO_BUT_ID", 1),
      ("GEARBOX", 1),
      ("WARNING_SIGNALS", 1),
      ("STEER_MOMENT_SENSOR", 1),
      ("ACC_STATUS", 1),
      ("BSW_STATUS", 1),
      ("JOYSTICK_COMMAND", 1),
    ]
    return CANParser(DBC[CP.carFingerprint]["pt"], signals, checks, 0)
