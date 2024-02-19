// global torque limit
const int MITSUBISHI_MAX_TORQUE = 1500;       // max torque cmd allowed ever

// rate based torque limit + stay within actually applied
// packet is sent at 100hz, so this limit is 1000/sec
const int MITSUBISHI_MAX_RATE_UP = 10;        // ramp up slow
const int MITSUBISHI_MAX_RATE_DOWN = 25;      // ramp down fast
const int MITSUBISHI_MAX_TORQUE_ERROR = 350;  // max torque cmd in excess of torque motor

// real time torque limit to prevent controls spamming
// the real time limit is 1500/sec
const int MITSUBISHI_MAX_RT_DELTA = 375;      // max delta torque allowed for real time checks
const uint32_t MITSUBISHI_RT_INTERVAL = 250000;    // 250ms between real time checks

// longitudinal limits
const int MITSUBISHI_MAX_ACCEL = 2000;        // 2.0 m/s2
const int MITSUBISHI_MIN_ACCEL = -3500;       // -3.5 m/s2

const int MITSUBISHI_STANDSTILL_THRSLD = 100;  // 1kph

// Roughly calculated using the offsets in openpilot +5%: Controls Unresposive
// In openpilot: ((gas1_norm + gas2_norm)/2) > 15
// gas_norm1 = ((gain_dbc*gas1) + offset1_dbc)
// gas_norm2 = ((gain_dbc*gas2) + offset2_dbc)
// In this safety: ((gas1 + gas2)/2) > THRESHOLD
const int MITSUBISHI_GAS_INTERCEPTOR_THRSLD = 845;
#define MITSUBISHI_GET_INTERCEPTOR(msg) (((GET_BYTE((msg), 0) << 8) + GET_BYTE((msg), 1) + (GET_BYTE((msg), 2) << 8) + GET_BYTE((msg), 3)) / 2) // avg between 2 tracks

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
//    // enter controls on rising edge of ACC, exit controls on ACC off
//    if (addr == 0x240) {
//      int cruise_engaged = ((GET_BYTES_48(to_push) >> 9) & 1U);
//      if (cruise_engaged && !cruise_engaged_prev) {
//        controls_allowed = 1;
//      }
//      if (!cruise_engaged) {
//        controls_allowed = 0;
//      }
//      cruise_engaged_prev = cruise_engaged;
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
    if (longitudinal_allowed)
	tx = 0;

    if (GET_ADDR(to_send) == 0x3b6)
        tx = 1;

//  if (relay_malfunction) {
//    tx = 0;
//  }

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
