
from selfdrive.car import apply_std_steer_torque_limits
from opendbc.can.packer import CANPacker
#from common.dp_common import common_controller_ctrl
from selfdrive.car.mitsubishi.values import CAR, CarControllerParams

import cereal.messaging as messaging

class CarController():
  def __init__(self, dbc_name, CP, VM):
    # dp
    self.last_blinker_on = False
    self.blinker_end_frame = 0.

    self.last_steer = 0
    self.accel_steady = 0.
    self.alert_active = False
    self.last_standstill = False
    self.standstill_req = False
    self.steer_rate_limited = False
    self.use_interceptor = False
    self.gone_fast_yet = False
    self.apply_steer_last = 0
    self.packer = CANPacker(dbc_name)
    self.sm = None
    self.cnt = 0

  def create_lkas_command(self, apply_steer, apply_angle, active, ll, rl, lc, sr, sf, anoffs, frame):
    values = {
      "LKAS_TEST_DATA_2": sf,
      "LKAS_ANGLE_OFFSET": anoffs,
      "LKAS_LEAD_CAR": lc,
      "LKAS_STEERING_TORQUE": sr, #apply_steer,
      "LKAS_STEERING_ANGLE": apply_angle,
      "LKAS_ACTIVE": active,
      "LKAS_RIGHT_LINE": rl,
      "LKAS_LEFT_LINE": ll,
      "COUNTER": frame % 0x10,
    }
    #print ("sr=%d" % (sr)) # dmonitoringd

    return self.packer.make_can_msg("LKAS_COMMAND", 0, values)

  def update(self, enabled, CS, frame, actuators, pcm_cancel_cmd, hud_alert,
               left_line, right_line, lead, left_lane_depart, right_lane_depart):

    can_sends = []

    if self.sm is None:
       self.sm = messaging.SubMaster(['liveParameters','carState']) #sm['carState'].yawRate
    else:
      self.sm.update(0)

    angleOffset = int(round(self.sm['liveParameters'].angleOffsetDeg * 10))

    steerRatio = int(round(self.sm['liveParameters'].steerRatio * 10))
    #steerRatio = int(actuators.accel)

    #stiff = int(round(self.sm['liveParameters'].stiffnessFactor  * 100))
    #stiff = int(round(self.sm['liveParameters'].roll * 10))
    sad = int(round(self.sm['carState'].newSteerActuatorDelay*500))
    CS.CP.steerActuatorDelay = self.sm['carState'].newSteerActuatorDelay


    new_steer = int(round(actuators.steer * CarControllerParams.STEER_MOMENT_MAX))
    new_steer = -new_steer

    apply_steer = apply_std_steer_torque_limits(new_steer, self.apply_steer_last,
                                                   CS.out.steeringTorqueEps, CarControllerParams)

    print ("ll=%d, rl=%d lead=%d sr=%d sf=%d tst=%d" % (left_line, right_line, lead, steerRatio, sad, angleOffset)) # dmonitoringd

    
    new_msg = self.create_lkas_command(int(apply_steer), int(actuators.steeringAngleDeg*2),
                        int(enabled), int(left_line), int(right_line), int(lead), steerRatio, sad, angleOffset, frame)

    #can_sends.append(self.packer.make_can_msg(921, b'\x00\x00\x00\x00\x00\x00\x00\x00', 0))
    can_sends.append(new_msg)

    self.apply_steer_last = apply_steer
    #can_sends.append((0x18DAB0F1, 0, b"\x02\x3E\x80\x00\x00\x00\x00\x00", 0))

    new_actuators = actuators.copy()
    #new_actuators.steer = apply_steer / CarControllerParams.STEER_MAX
    # new_actuators.accel = self.accel
    # new_actuators.gas = self.gas

    return new_actuators, can_sends
