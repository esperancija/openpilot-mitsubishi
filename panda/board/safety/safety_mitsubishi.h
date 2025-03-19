

const CanMsg MITSUBISHI_TX_MSGS[] = {{0x3b6, 0, 8}};  // interceptor

//AddrCheckStruct mitsubishi_addr_checks[] = {
//  {.msg = {{ 0xaa, 0, 8, .check_checksum = false, .expected_timestep = 12000U}, { 0 }, { 0 }}},
//  {.msg = {{0x260, 0, 8, .check_checksum = true, .expected_timestep = 20000U}, { 0 }, { 0 }}},
//  {.msg = {{0x1D2, 0, 8, .check_checksum = true, .expected_timestep = 30000U}, { 0 }, { 0 }}},
//  {.msg = {{0x224, 0, 8, .check_checksum = false, .expected_timestep = 25000U},
//           {0x226, 0, 8, .check_checksum = false, .expected_timestep = 25000U}, { 0 }}},
//};
//#define MITSUBISHI_ADDR_CHECKS_LEN (sizeof(mitsubishi_addr_checks) / sizeof(mitsubishi_addr_checks[0]))
//addr_checks mitsubishi_rx_checks = {mitsubishi_addr_checks, MITSUBISHI_ADDR_CHECKS_LEN};

// global actuation limit states
int mitsubishi_dbc_eps_torque_factor = 100;   // conversion factor for STEER_TORQUE_EPS in %: see dbc file

//static uint8_t mitsubishi_compute_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
//  int addr = GET_ADDR(to_push);
//  int len = GET_LEN(to_push);
//  uint8_t checksum = (uint8_t)(addr) + (uint8_t)((unsigned int)(addr) >> 8U) + (uint8_t)(len);
//  for (int i = 0; i < (len - 1); i++) {
//    checksum += (uint8_t)GET_BYTE(to_push, i);
//  }
//  return checksum;
//}
//
//static uint8_t mitsubishi_get_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
//  int checksum_byte = GET_LEN(to_push) - 1;
//  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
//}

static int mitsubishi_rx_hook(CANPacket_t *to_push) {

    int addr = GET_ADDR(to_push);
//    int len = GET_LEN(to_push);
//    int bus = GET_BUS(to_push);

	if (addr == STEERING_WHEEL_POS_ID){
//			murchik.steerPosition = (((CAN->sFIFOMailBox[0].RDLR & 0xff) << 8) +
//											((CAN->sFIFOMailBox[0].RDLR >> 8) & 0xff)) - 0x1000;//0x0ffd;//0x1000;
		    mishka.steerPosition = ((GET_BYTE(to_push, 0) << 8) + GET_BYTE(to_push, 1)) - 0x1000;
		    mishka.flags |= runMomentCalcFlag;
	}else if (addr == SPEED_ID){
		//murchik.speed = 10*(((CAN->sFIFOMailBox[0].RDLR & 0xff) << 8) + ((CAN->sFIFOMailBox[0].RDLR >> 8) & 0xff))/12;
		mishka.speed = ((GET_BYTE(to_push, 0) << 8) + GET_BYTE(to_push, 1))/12;

	}else if (addr == STEER_CONTROL_ID){
		mishka.opActiveTimer = OP_ACTIVE_TIMEOUT;
		//mishka.steerTargetAngle = ((CAN->sFIFOMailBox[0].RDLR >> 16) & 0x7ff) - 1024;
		mishka.steerTargetAngle = (((GET_BYTE(to_push, 3) << 8) + GET_BYTE(to_push, 2)) & 0x7ff) - 1024;
		mishka.steerMoment = (((GET_BYTE(to_push, 1) << 8) + GET_BYTE(to_push, 0)) & 0x7ff) - 1024;

		//mishka.opData = ((CAN->sFIFOMailBox[0].RDLR >> 11) & 0x1f);
		mishka.opData = ((GET_BYTE(to_push, 1) >> 3) & 0x1f);

//		murchik.accTest1 = ((CAN->sFIFOMailBox[0].RDHR) & 0xff);
//		murchik.accTest2 = ((CAN->sFIFOMailBox[0].RDHR >> 8) & 0xff);
//	#if (CONTROL_MODE == MOMENT_CONTROL)
//		murchik.steerTargetMoment = ((CAN->sFIFOMailBox[0].RDLR) & 0x7ff) - 1024;
//	#endif
	}

//    // enter controls on rising edge of ACC, exit controls on ACC off
//    if (addr == 0x240) {
//      int cruise_engaged = ((GET_BYTES_48(to_push) >> 9) & 1U);
//      if (cruise_engaged && !cruise_engaged_prev) {
//        controls_allowed = 1;
//      }
//      if (!cruise_engaged) {
//        controls_allowed = 0;
//      }
//      cruise_engaged_prev = cruise_engaged; ->rx
//    }
  controls_allowed = 1;
  UNUSED(to_push);
  return true;
}

//static int mitsubishi_tx_hook(CANPacket_t *to_send) {
static int mitsubishi_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {

  int tx = 0;
//  int addr = GET_ADDR(to_send);
//  int bus = GET_BUS(to_send);

//  if (!msg_allowed(to_send, MITSUBISHI_TX_MSGS, sizeof(MITSUBISHI_TX_MSGS)/sizeof(MITSUBISHI_TX_MSGS[0]))) {
//    tx = 0;
//  }
//    if (longitudinal_allowed)
//	tx = 0;
  	UNUSED(longitudinal_allowed);

    if (GET_ADDR(to_send) == 0x3b6)
        tx = 1;

  if (relay_malfunction) {
    tx = 0;
  }

  return tx;
}

static const addr_checks* mitsubishi_init(int16_t param) {
//  controls_allowed = 0;
//  relay_malfunction_reset();
//  gas_interceptor_detected = 0;
//  mitsubishi_dbc_eps_torque_factor = param;
//  return &mitsubishi_rx_checks;
  UNUSED(param);
  controls_allowed = true;
  relay_malfunction_reset();
  return &default_rx_checks;
}

static int mitsubishi_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  UNUSED(to_fwd);
  UNUSED(bus_num);

  return -1;
}

const safety_hooks mitsubishi_hooks = {
  .init = mitsubishi_init,
  .rx = mitsubishi_rx_hook,
  .tx = mitsubishi_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = mitsubishi_fwd_hook,
};
