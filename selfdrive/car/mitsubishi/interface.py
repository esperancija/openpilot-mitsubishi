#!/usr/bin/env python3
from cereal import car
from common.conversions import Conversions as CV
from selfdrive.car import STD_CARGO_KG, scale_rot_inertia, scale_tire_stiffness, gen_empty_fingerprint, get_safety_config
from selfdrive.car.interfaces import CarInterfaceBase
from selfdrive.car.mitsubishi.values import MIN_ACC_SPEED, CarControllerParams
#from common.dp_common import common_interface_atl, common_interface_get_params_lqr

EventName = car.CarEvent.EventName


class CarInterface(CarInterfaceBase):
  def __init__(self, CP, CarController, CarState):
    super().__init__(CP, CarController, CarState)

    self.oldCruiseState = 0.

  @staticmethod
  def get_pid_accel_limits(CP, current_speed, cruise_speed):
    return CarControllerParams.ACCEL_MIN, CarControllerParams.ACCEL_MAX

  @staticmethod
  def get_params(candidate, fingerprint=gen_empty_fingerprint(), car_fw=[], disable_radar=False):  # pylint: disable=dangerous-default-value Here 2!!!
    ret = CarInterfaceBase.get_std_params(candidate, fingerprint)

    ret.carName = "mitsubishi"
    ret.radarOffCan = True
    ret.lateralTuning.init('pid')

    ret.safetyConfigs = [get_safety_config(car.CarParams.SafetyModel.mitsubishi)]
    #ret.safetyConfigs[0].safetyParam = 1 #EPS_SCALE[candidate] 0x399 modelV2

    ret.steerLimitTimer = 1 #0.4
    ret.steerRateCost = 0.05 #0.02 #0.15

    ret.steerActuatorDelay = 0.3 #0.45 #0.55 #5 #0.05
    ret.steerRatio = 16.5 #18 #17 #11 #18 # 14 #10.15  #12.4 #13.00
    tire_stiffness_factor = 0.9 #0.7933

	

    ret.lateralTuning.pid.kf = 0. #0.000039
    #ret.lateralTuning.pid.kiBP, ret.lateralTuning.pid.kpBP = [[0., 10., 20.], [0., 10., 20.]]
    #ret.lateralTuning.pid.kpV, ret.lateralTuning.pid.kiV = [[0.01, 0.05, 0.2], [0.003, 0.018, 0.025]]
    ret.lateralTuning.pid.kiBP, ret.lateralTuning.pid.kpBP = [[0., 20.], [0.,20.]]
    ret.lateralTuning.pid.kpV, ret.lateralTuning.pid.kiV = [[0., 0.], [0., 0.]]


    ret.stoppingControl = False  # Toyota starts braking more when it thinks you want to stop

    stop_and_go = False

    ret.mass = 1800 + STD_CARGO_KG
    ret.wheelbase = 2.68986
    ret.centerToFront = ret.wheelbase * 0.5

    #tire_stiffness_factor = 0.7933
    #set_lat_tune(ret.lateralTuning, LatTunes.PID_D)


    # TODO: get actual value, for now starting with reasonable value for
    # civic and scaling by mass and wheelbase
    ret.rotationalInertia = scale_rot_inertia(ret.mass, ret.wheelbase)

    # TODO: start from empirically derived lateral slip stiffness for the civic and scale by
    # mass and CG position, so all cars will have approximately similar dyn behaviors
    ret.tireStiffnessFront, ret.tireStiffnessRear = scale_tire_stiffness(ret.mass, ret.wheelbase, ret.centerToFront,
                                                                         tire_stiffness_factor=tire_stiffness_factor)

    # if the smartDSU is detected, openpilot can send ACC_CMD (and the smartDSU will block it from the DSU) or not (the DSU is "connected")
    ret.openpilotLongitudinalControl = True #smartDsu or ret.enableDsu or candidate in TSS2_CAR

    # min speed to enable ACC. if car can do stop and go, then set enabling speed
    # to a negative value, so it won't matter.
    ret.minEnableSpeed = -1. if (stop_and_go or ret.enableGasInterceptor) else MIN_ACC_SPEED

    # dp
    #ret = common_interface_get_params_lqr(ret)

    #set_long_tune(ret.longitudinalTuning, LongTunes.PEDAL)
    return ret

  # returns a car.CarState
  def update(self, c, can_strings):

    # ******************* do can recv *******************
    self.cp.update_strings(can_strings)
    #self.cp_cam.update_strings(can_strings)

    ret = self.CS.update(self.cp, self.cp_cam)

    events = self.create_common_events(ret)

    if ((ret.cruiseState.enabled == 1) and (self.oldCruiseState != ret.cruiseState.enabled)):
      events.add(EventName.buttonEnable)
      #print("Try send enable event %d and %d" % (ret.cruiseState.enabled, self.oldCruiseState))

    if ((ret.cruiseState.enabled == 0) and (self.oldCruiseState != ret.cruiseState.enabled)):
      events.add(EventName.buttonCancel)

    self.oldCruiseState = ret.cruiseState.enabled

    ret.canValid = self.cp.can_valid #and self.cp_cam.can_valid
    ret.steeringRateLimited = self.CC.steer_rate_limited if self.CC is not None else False

    ret.events = events.to_msg()

    self.CS.out = ret.as_reader()
    return self.CS.out

  # pass in a car.CarControl
  # to be called @ 100hz
  def apply(self, c):
    hud_control = c.hudControl

    ret = self.CC.update(c.enabled, self.CS, self.frame,
                         c.actuators, c.cruiseControl.cancel,
                         hud_control.visualAlert, hud_control.leftLaneVisible,
                         hud_control.rightLaneVisible, hud_control.leadVisible,
                         hud_control.leftLaneDepart, hud_control.rightLaneDepart)

    self.frame += 1
    return ret
