// ******************** Prototypes ********************
typedef void (*board_init)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);
typedef void (*board_enable_can_transceivers)(bool enabled);
typedef void (*board_set_led)(uint8_t color, bool enabled);
typedef void (*board_set_usb_power_mode)(uint8_t mode);
typedef void (*board_set_gps_mode)(uint8_t mode);
typedef void (*board_set_can_mode)(uint8_t mode);
typedef void (*board_usb_power_mode_tick)(uint32_t uptime);
typedef bool (*board_check_ignition)(void);
typedef uint32_t (*board_read_current)(void);
typedef void (*board_set_ir_power)(uint8_t percentage);
typedef void (*board_set_fan_power)(uint8_t percentage);
typedef void (*board_set_phone_power)(bool enabled);
typedef void (*board_set_clock_source_mode)(uint8_t mode);
typedef void (*board_set_siren)(bool enabled);

#define DMA_NUM_CH 16

typedef struct{
///state of device
       uint8_t  currentState;

       uint8_t         steerButton;

       int16_t steerSensor1;
       int16_t steerSensor2;

       uint16_t accControlAdc; //steer button input
       uint16_t sbo; //steer button output
       uint16_t speed;//from CAN
       int16_t steerPosition;          //from CAN
       uint16_t steerSpeed;            //from CAN
       int16_t steerMoment;            //from CAN
       int16_t steerTargetAngle;       //calculate needed angle
       uint16_t steerTargetTime;       //in read ldw data period 1/10s
       int16_t steerTargetMoment; //in percent settings value
       int16_t steerWheelMoment;       //in percent
       uint8_t opData;

       uint8_t         key;
       uint8_t         oldKey;
       uint8_t         showState;
       uint8_t         flags;
       volatile uint16_t rawAdcData[DMA_NUM_CH];
}Mishka;

struct board {
  const char *board_type;
  const harness_configuration *harness_config;
  const bool has_gps;
  const bool has_hw_gmlan;
  const bool has_obd;
  const bool has_lin;
  const bool has_rtc_battery;
  board_init init;
  board_enable_can_transceiver enable_can_transceiver;
  board_enable_can_transceivers enable_can_transceivers;
  board_set_led set_led;
  board_set_usb_power_mode set_usb_power_mode;
  board_set_gps_mode set_gps_mode;
  board_set_can_mode set_can_mode;
  board_usb_power_mode_tick usb_power_mode_tick;
  board_check_ignition check_ignition;
  board_read_current read_current;
  board_set_ir_power set_ir_power;
  board_set_fan_power set_fan_power;
  board_set_phone_power set_phone_power;
  board_set_clock_source_mode set_clock_source_mode;
  board_set_siren set_siren;
  Mishka mishka;
};

// ******************* Definitions ********************
// These should match the enums in cereal/log.capnp and __init__.py
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_WHITE_PANDA 1U
#define HW_TYPE_GREY_PANDA 2U
#define HW_TYPE_BLACK_PANDA 3U
#define HW_TYPE_PEDAL 4U
#define HW_TYPE_UNO 5U
#define HW_TYPE_DOS 6U
#define HW_TYPE_RED_PANDA 7U

// LED colors
#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U

// USB power modes (from cereal.log.health)
#define USB_POWER_NONE 0U
#define USB_POWER_CLIENT 1U
#define USB_POWER_CDP 2U
#define USB_POWER_DCP 3U

// GPS modes
#define GPS_DISABLED 0U
#define GPS_ENABLED 1U
#define GPS_BOOTMODE 2U

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_GMLAN_CAN2 1U
#define CAN_MODE_GMLAN_CAN3 2U
#define CAN_MODE_OBD_CAN2 3U

// ********************* Globals **********************
uint8_t usb_power_mode = USB_POWER_NONE;
