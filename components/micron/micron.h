#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome
{
  namespace micron
  {
    // Micron board type constants
    static const uint8_t MICRON_TYPE_UNKNOWN = 0;
    static const uint8_t MICRON_TYPE_8ZONE   = 8;
    static const uint8_t MICRON_TYPE_16ZONE  = 16;

    // TIming variables
    static const uint32_t MICRON_CLOCK_TIMEOUT_MS = 50;
    static const uint32_t MICRON_MIN_US = 20; // originally it was 100;
    static const uint32_t MICRON_MAX_MS = 18; // originally it was 30
    
    static const uint8_t MICRON_PACKET_LEN = 5; //was 3 but changed to 5 to support 16 zone version
    // Frame and packet length size is different for the 8 and 16 Zone versions
    static const uint8_t MICRON_FRAME_SIZE_8ZONE = 24;
    static const uint8_t MICRON_PACKET_LEN_8ZONE = 3;
    static const uint8_t MICRON_FRAME_SIZE_16ZONE = 40;
    static const uint8_t MICRON_PACKET_LEN_16ZONE = 5;

    static const uint8_t MICRON_COMMAND_FRAME_SIZE = 8; // was 7 originally, but 8 bit commands are required
    // Old values for 8 zone verison
    // static const uint8_t MICRON_BYTE_HIGH = 1;
    // static const uint8_t MICRON_BYTE_LOW = 2;
    // changes for 16 zone support
    static const uint8_t MICRON_BYTE_COMMAND = 0;
    static const uint8_t MICRON_BYTE_DATA_1 = 1;
    static const uint8_t MICRON_BYTE_DATA_2 = 2;
    // 3 and 4 only needed for the 16 zone version
    static const uint8_t MICRON_BYTE_DATA_3 = 3;
    static const uint8_t MICRON_BYTE_DATA_4 = 4;

    //Keypad data values
    static const uint16_t MICRON_KEYPAD_1 = 0x48;
    static const uint16_t MICRON_KEYPAD_2 = 0x28;
    static const uint16_t MICRON_KEYPAD_3 = 0x18;
    static const uint16_t MICRON_KEYPAD_4 = 0x44;
    static const uint16_t MICRON_KEYPAD_5 = 0x24;
    static const uint16_t MICRON_KEYPAD_6 = 0x14;
    static const uint16_t MICRON_KEYPAD_7 = 0x42;
    static const uint16_t MICRON_KEYPAD_8 = 0x22;
    static const uint16_t MICRON_KEYPAD_9 = 0x12;
    static const uint16_t MICRON_KEYPAD_STAR = 0x41;
    static const uint16_t MICRON_KEYPAD_0 = 0x21;
    static const uint16_t MICRON_KEYPAD_HASH = 0x11;
    static const uint16_t MICRON_KEYPAD_F1 = 0x09;
    static const uint16_t MICRON_KEYPAD_F2 = 0x0a;
    static const uint16_t MICRON_KEYPAD_F3 = 0x0b;

    // Zone mask values
    // currently only the 8 zone versions are identified
    static const uint32_t MICRON_ZONE_1_MASK = 0x0001;
    static const uint32_t MICRON_ZONE_2_MASK = 0x0002;
    static const uint32_t MICRON_ZONE_3_MASK = 0x0004;
    static const uint32_t MICRON_ZONE_4_MASK = 0x0008;
    static const uint32_t MICRON_ZONE_5_MASK = 0x0010;
    static const uint32_t MICRON_ZONE_6_MASK = 0x0020;
    static const uint32_t MICRON_ZONE_A_MASK = 0x0040;
    static const uint32_t MICRON_KEY_BEEP_3_MASK = 0x0080;
    static const uint32_t MICRON_ZONE_7_MASK = 0x0100;
    static const uint32_t MICRON_ZONE_8_MASK = 0x0200;
    static const uint32_t MICRON_NIGHT_ARMED_MASK = 0x0400;
    static const uint32_t MICRON_0800_MASK = 0x0800;
    static const uint32_t MICRON_BATTERY_MASK = 0x1000;
    static const uint32_t MICRON_M_MASK = 0x2000;
    static const uint32_t MICRON_ZONE_B_MASK = 0x4000;
    static const uint32_t MICRON_KEY_BEEP_1_MASK = 0x8000;

    static const uint16_t MICRON_SIREN_MASK = 0x0001;

    static const uint32_t MICRON_MAX_COMMAND_DELAY_MS = 240;

    uint8_t key_to_command(const char key);
    std::vector<uint8_t> keys_to_commands(const std::string &keys);

    struct MicronPacket {
      uint8_t command;
      //uint16_t status;
      // status is changed to 32bit to support 16 zone version
      uint32_t status;
    };

    class MicronDataProcessor {
    public:
      void next(uint32_t ms);
      void write(ISRInternalGPIOPin *pin_data_out);
      bool decode(uint32_t ms, bool data, int8_t frame_size);
      MicronPacket *packet = new MicronPacket;
      uint8_t command_out = 0;
      uint8_t command_repeat = 0;
      uint8_t remaining_command_writes = 0;

    protected:
      uint8_t buffer_[MICRON_PACKET_LEN_16ZONE]; // reserved for the 16 zone version 
      int num_bits_ = 0;
      uint32_t prev_ms_;
    };

    struct MicronStore {
    public:
      uint8_t command;
      //uint16_t status;
      // status is changed to 32bit to support 16 zone version
      uint32_t status;
      uint16_t siren; //bit 0 will store siren input status

      uint32_t interrupts = 0;
      uint32_t bits_received = 0;
      uint32_t packet_interrupts = 0;
      uint32_t packet_bits = 0;
      uint32_t packets_received = 0;
      uint32_t packets_with_interference = 0;
      uint32_t commands_sent = 0;
      uint8_t alarm_board_type = MICRON_TYPE_UNKNOWN;
      uint8_t frame_size = 0;
      // variables used to identify the bord type
      uint8_t id_cycle_count = 4; // 4 cycles are used to identify the board type
      uint8_t id_clock_count = 0; // count of clock cycles per packet

      uint32_t last_packet_ms;

      void setup(InternalGPIOPin *pin_clock, InternalGPIOPin *pin_data, InternalGPIOPin *pin_data_out, InternalGPIOPin *pin_siren, InternalGPIOPin *pin_siren_out);
      void write(uint8_t command, uint8_t repeat = 1);
      static void interrupt(MicronStore *arg);
      static void interruptID(MicronStore *arg);

      MicronDataProcessor processor_;
    protected:
      ISRInternalGPIOPin pin_clock_;
      ISRInternalGPIOPin pin_data_;
      ISRInternalGPIOPin pin_data_out_;
      ISRInternalGPIOPin pin_siren_;
      ISRInternalGPIOPin pin_siren_out_;


      uint32_t last_interrupt_us_;

      void set_data_(MicronPacket *packet);
    };

    class MicronComponent : public PollingComponent
    {
    public:
      void set_pin_clock(InternalGPIOPin *pin_clock) { pin_clock_ = pin_clock; }
      void set_pin_data(InternalGPIOPin *pin_data) { pin_data_ = pin_data; }
      void set_pin_data_out(InternalGPIOPin *pin_data_out) { pin_data_out_ = pin_data_out; }
      void set_pin_siren(InternalGPIOPin *pin_siren) { pin_siren_ = pin_siren; }
      void set_pin_siren_out(InternalGPIOPin *pin_siren_out) { pin_siren_out_ = pin_siren_out; }

      void set_connected_binary_sensor(binary_sensor::BinarySensor  *connected_binary_sensor) { connected_binary_sensor_ = connected_binary_sensor; }

      void set_m_binary_sensor(binary_sensor::BinarySensor  *m_binary_sensor) { m_binary_sensor_ = m_binary_sensor; }
      void set_battery_binary_sensor(binary_sensor::BinarySensor  *battery_binary_sensor) { battery_binary_sensor_ = battery_binary_sensor; }
      void set_zonea_binary_sensor(binary_sensor::BinarySensor  *zonea_binary_sensor) { zonea_binary_sensor_ = zonea_binary_sensor; }
      void set_zoneb_binary_sensor(binary_sensor::BinarySensor  *zoneb_binary_sensor) { zoneb_binary_sensor_ = zoneb_binary_sensor; }

      void set_beep1_binary_sensor(binary_sensor::BinarySensor  *beep1_binary_sensor) { beep1_binary_sensor_ = beep1_binary_sensor; }
      void set_beep3_binary_sensor(binary_sensor::BinarySensor  *beep3_binary_sensor) { beep3_binary_sensor_ = beep3_binary_sensor; }

      void set_zone1_binary_sensor(binary_sensor::BinarySensor  *zone1_binary_sensor) { zone1_binary_sensor_ = zone1_binary_sensor; }
      void set_zone2_binary_sensor(binary_sensor::BinarySensor  *zone2_binary_sensor) { zone2_binary_sensor_ = zone2_binary_sensor; }
      void set_zone3_binary_sensor(binary_sensor::BinarySensor  *zone3_binary_sensor) { zone3_binary_sensor_ = zone3_binary_sensor; }
      void set_zone4_binary_sensor(binary_sensor::BinarySensor  *zone4_binary_sensor) { zone4_binary_sensor_ = zone4_binary_sensor; }
      void set_zone5_binary_sensor(binary_sensor::BinarySensor  *zone5_binary_sensor) { zone5_binary_sensor_ = zone5_binary_sensor; }
      void set_zone6_binary_sensor(binary_sensor::BinarySensor  *zone6_binary_sensor) { zone6_binary_sensor_ = zone6_binary_sensor; }
      void set_zone7_binary_sensor(binary_sensor::BinarySensor  *zone7_binary_sensor) { zone7_binary_sensor_ = zone7_binary_sensor; }
      void set_zone8_binary_sensor(binary_sensor::BinarySensor  *zone8_binary_sensor) { zone8_binary_sensor_ = zone8_binary_sensor; }

      void set_keypad_text_sensor(text_sensor::TextSensor  *keypad_text_sensor) { keypad_text_sensor_ = keypad_text_sensor; }
      void set_status_text_sensor(text_sensor::TextSensor  *status_text_sensor) { status_text_sensor_ = status_text_sensor; }
      void set_siren_binary_sensor(binary_sensor::BinarySensor  *siren_binary_sensor) { siren_binary_sensor_ = siren_binary_sensor; }
      
      void set_night_binary_sensor(binary_sensor::BinarySensor  *night_binary_sensor) { night_binary_sensor_ = night_binary_sensor; }
      void set_test2_binary_sensor(binary_sensor::BinarySensor  *test2_binary_sensor) { test2_binary_sensor_ = test2_binary_sensor; }

      uint8_t last_command();

      void write(uint8_t command);
      void write(std::vector<uint8_t> commands);
      void press(const std::string &keys);

      // ========== INTERNAL METHODS ==========
      // (In most use cases you won't need these)
      void setup() override;
      void dump_config() override;
      void loop() override;
      void update() override;
      float get_setup_priority() const override;
    protected:
      MicronStore store_;
      InternalGPIOPin *pin_clock_;
      InternalGPIOPin *pin_data_;
      InternalGPIOPin *pin_data_out_;
      InternalGPIOPin *pin_siren_;
      InternalGPIOPin *pin_siren_out_;

      binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};

      binary_sensor::BinarySensor *m_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *battery_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zonea_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zoneb_binary_sensor_{nullptr};

      binary_sensor::BinarySensor *beep1_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *beep3_binary_sensor_{nullptr};

      binary_sensor::BinarySensor *zone1_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone2_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone3_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone4_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone5_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone6_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone7_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *zone8_binary_sensor_{nullptr};

      text_sensor::TextSensor *keypad_text_sensor_{nullptr};
      text_sensor::TextSensor *status_text_sensor_{nullptr};
      binary_sensor::BinarySensor *siren_binary_sensor_{nullptr};

      binary_sensor::BinarySensor *night_binary_sensor_{nullptr};
      binary_sensor::BinarySensor *test2_binary_sensor_{nullptr};

      Deduplicator<uint8_t> command_dedupe_;
      Deduplicator<uint16_t> status_dedupe_;

      uint32_t last_command_ms_ = 0;
      uint8_t last_command_ = 0;
      std::queue<uint8_t> command_queue_;
    };

  } // namespace micron
} // namespace esphome
