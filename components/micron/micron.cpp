#include "micron.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace micron
  {

    static const char *const TAG = "micron";

    uint8_t key_to_command(const char key) {
      // ESP_LOGD("key_to_command", "Key: %x", key);
      switch (key) {
        case '1': return MICRON_KEYPAD_1;
        case '2': return MICRON_KEYPAD_2;
        case '3': return MICRON_KEYPAD_3;
        case '4': return MICRON_KEYPAD_4;
        case '5': return MICRON_KEYPAD_5;
        case '6': return MICRON_KEYPAD_6;
        case '7': return MICRON_KEYPAD_7;
        case '8': return MICRON_KEYPAD_8;
        case '9': return MICRON_KEYPAD_9;
        case '0': return MICRON_KEYPAD_0;
        case '*': return MICRON_KEYPAD_STAR;
        case '#': return MICRON_KEYPAD_HASH;
      }
      return 0;
    }

    std::vector<uint8_t> keys_to_commands(const std::string &keys) {
      //ESP_LOGD("keys_to_commands", "Keys: %s", keys.c_str());

      std::vector<uint8_t> commands;
      for(char c : keys) {
        //ESP_LOGD("keys_to_command", "Key: %c", c);
        commands.push_back(key_to_command(c));
      }
      return commands;
    }

    void IRAM_ATTR MicronDataProcessor::next(uint32_t ms) {
      // check if a new message has started, based on time since previous bit
      if ((ms - this->prev_ms_) > MICRON_MAX_MS) {
        this->num_bits_ = 0;

        if (this->command_repeat > 0) {
          // more repeats. restart write bits
          this->remaining_command_writes = MICRON_COMMAND_FRAME_SIZE; // + 1; 
        }
      }

      this->prev_ms_ = ms;
    }

    void IRAM_ATTR MicronDataProcessor::write(ISRInternalGPIOPin *pin_data_out) {

      if (this->remaining_command_writes > 0) {
        // some bits to write remaining

        if (this->num_bits_ < MICRON_COMMAND_FRAME_SIZE) {
          uint8_t bit_mask = 1 << (MICRON_COMMAND_FRAME_SIZE - this->num_bits_ - 1);
          bool out_bit = (this->command_out & bit_mask) == bit_mask;

          pin_data_out->digital_write(out_bit);
        } else {
          // turn back data to 0
          pin_data_out->digital_write(false);

          if (this->command_repeat > 0)
            this->command_repeat--;
        }

        this->remaining_command_writes--;
      } else {
        pin_data_out->digital_write(false);
      }

      //pin_data_out->digital_write(this->num_bits_ % 2 == 0);
    }

    bool IRAM_ATTR MicronDataProcessor::decode(uint32_t ms, bool data, int8_t frame_size) {

      // number of bits received is basically the "state"
      if (this->num_bits_ < frame_size) {
        // store it while it fits
        int idx = this->num_bits_ / 8;
        this->buffer_[idx] = (this->buffer_[idx] << 1) | (data ? 1 : 0);
        this->num_bits_++;

        // are we done yet?
        if (this->num_bits_ == frame_size) {

          this->packet->command = this->buffer_[MICRON_BYTE_COMMAND] ; // >> 1; I think i need 8 bits in the commands
          if (frame_size == MICRON_FRAME_SIZE_8ZONE) {
            this->buffer_[MICRON_BYTE_DATA_3] = 0;
            this->buffer_[MICRON_BYTE_DATA_4] = 0;
          }
          this->packet->status = this->buffer_[MICRON_BYTE_DATA_3] << 24 | this->buffer_[MICRON_BYTE_DATA_4] << 16 | this->buffer_[MICRON_BYTE_DATA_1] << 8 | this->buffer_[MICRON_BYTE_DATA_2];

          return true;
        }
      }
      return false;
    }

    void MicronStore::setup(InternalGPIOPin *pin_clock, InternalGPIOPin *pin_clock2, InternalGPIOPin *pin_data, InternalGPIOPin *pin_data_out, InternalGPIOPin *pin_siren, InternalGPIOPin *pin_siren_out) {
      pin_clock->setup();
      pin_clock2->setup();
      pin_data->setup();
      pin_data_out->setup();
      pin_siren->setup();
      pin_siren_out->setup();
      this->pin_clock_ = pin_clock->to_isr();
      this->pin_clock2_ = pin_clock2->to_isr();
      this->pin_data_ = pin_data->to_isr();
      this->pin_data_out_ = pin_data_out->to_isr();
      this->pin_siren_ = pin_siren->to_isr();
      this->pin_siren_out_ = pin_siren_out->to_isr();
      // Writing command and reading status should be on falling edge, however reading the commands from the keyboard
      // should happen on rising edge
      // TODO: create a separate interrupt routing just for reading keyboard commands
      //pin_clock->attach_interrupt(MicronStore::interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
      //pin_clock->attach_interrupt(MicronStore::interrupt, this, gpio::INTERRUPT_RISING_EDGE);
      // TEST: Doing both edges to support both sending and receiving commands
      //pin_clock->attach_interrupt(MicronStore::interrupt, this, gpio::INTERRUPT_ANY_EDGE);
      pin_clock->attach_interrupt(MicronStore::interruptID, this, gpio::INTERRUPT_FALLING_EDGE);
    }

    void MicronStore::setupFall(InternalGPIOPin *pin_clock) {
      pin_clock->attach_interrupt(MicronStore::interruptFall, this, gpio::INTERRUPT_FALLING_EDGE);
    }

    void MicronStore::setupRise(InternalGPIOPin *pin_clock2) {
      pin_clock2->attach_interrupt(MicronStore::interruptRise, this, gpio::INTERRUPT_RISING_EDGE);
    }

    void MicronStore::write(uint8_t command, uint8_t repeat) {
      this->processor_.command_out = command;
      this->processor_.command_repeat = repeat;
      this->commands_sent++;
      //this->processor_.remaining_command_writes = MICRON_COMMAND_FRAME_SIZE + 1;
    }

    void IRAM_ATTR MicronStore::interruptID(MicronStore *arg) {
      uint32_t now_us = micros();
      //bool clock_bit = arg->pin_clock_.digital_read();
      if ((now_us - arg->last_interrupt_us_) < MICRON_MIN_US) {
        //too shorter delay between interrupts.
        // this is caused by us sending command back to the panel,
        // which seems to cause and issue on the clock line
        return;
      }
      // ESP_LOGD(TAG, "Clock bit: %d", clock_bit);
      arg->id_clock_count++;
      //ESP_LOGD(TAG, "now: %d, last: %d, max: %d", now_us, arg->last_interrupt_us_, MICRON_MAX_MS * 1000);
      if ((now_us - arg->last_interrupt_us_)  > (MICRON_MAX_MS * 1000)) {
        ESP_LOGD(TAG, "Cycle complete, cycle: %d, clock: %d", arg->id_cycle_count, arg->id_clock_count);
        arg->id_cycle_count--;
        if (arg->id_cycle_count == 0) {
          if (arg->id_clock_count == MICRON_FRAME_SIZE_8ZONE) {
            arg->alarm_board_type = MICRON_TYPE_8ZONE;
            arg->frame_size = MICRON_FRAME_SIZE_8ZONE;
            ESP_LOGD(TAG, "8 Zone");
          }
          else if (arg->id_clock_count == MICRON_FRAME_SIZE_16ZONE) {
            arg->alarm_board_type = MICRON_TYPE_16ZONE;
            arg->frame_size = MICRON_FRAME_SIZE_16ZONE;
            ESP_LOGD(TAG, "16 Zone");
          }
          else {
            // identification failed, let's retry
            arg->id_cycle_count = 4;
            arg->id_clock_count = 0;
            ESP_LOGD(TAG, "Failed");
          }
          //if (arg->alarm_board_type != MICRON_TYPE_UNKNOWN) {
            //change interrupt settings
          //  pin_clock_->attach_interrupt(MicronStore::interruptFall, this, gpio::INTERRUPT_FALLING_EDGE);
          //  pin_clock2_->attach_interrupt(MicronStore::interruptRise, this, gpio::INTERRUPT_RISING_EDGE);
          //}

        }
        arg->id_clock_count = 0;
      }
      arg->last_interrupt_us_ = now_us;
      //ESP_LOGD(TAG, "Last int: %d", arg->last_interrupt_us_);
    }


    void IRAM_ATTR MicronStore::interruptFall(MicronStore *arg) {
      // Falling edge interrupt
      arg->interrupts++;
      arg->packet_interrupts++;

      uint32_t now_us = micros();

      if ((now_us - arg->last_interrupt_us_) < MICRON_MIN_US) {
        //too shorter delay between interrupts.
        // this is caused by us sending command back to the panel,
        // which seems to cause and issue on the clock line
        return;
      }
      arg->last_interrupt_us_ = now_us;
      
      auto now_ms = millis();
      arg->processor_.next(now_ms);
      // write command
      arg->processor_.write(&arg->pin_data_out_);
      bool data_bit = arg->pin_data_.digital_read();
      arg->bits_received++;
      arg->packet_bits++;
      //ESP_LOGD(TAG, "PAcketBits: %d, Data: %d", arg->packet_bits, data_bit);
      if (arg->processor_.decode(now_ms, data_bit, arg->frame_size)) {
        //ESP_LOGD(TAG, "DECODE RETURNED TRUE, %d", arg->packet_bits);
        arg->last_packet_ms = now_ms;
        arg->packets_received++;
        if (arg->packet_interrupts > arg->packet_bits) {
          arg->packets_with_interference++;
        }
        arg->packet_interrupts = 0;
        arg->packet_bits = 0;
        arg->set_data_(arg->processor_.packet);
      }

      data_bit = arg->pin_siren_.digital_read();
      if (data_bit) {
        arg->siren = 0x0001;
      }
      else {
        arg->siren = 0x0000;
      }
    }

    void IRAM_ATTR MicronStore::interruptRise(MicronStore *arg) {
      uint32_t now_us = micros();
      // TBD rising edge to read keyboard input
    }
/*    
    void IRAM_ATTR MicronStore::interrupt(MicronStore *arg) {
      arg->interrupts++;
      arg->packet_interrupts++;

      uint32_t now_us = micros();
      bool clock_bit = arg->pin_clock_.digital_read();
      bool data_bit = arg->pin_data_.digital_read();

      if ((now_us - arg->last_interrupt_us_) < MICRON_MIN_US) {
        //too shorter delay between interrupts.
        // this is caused by us sending command back to the panel,
        // which seems to cause and issue on the clock line
        return;
      }
      // Read clock value:
      //  low -> falling edge -> Sens command, count number of clock cycles
      //  high -> rising edge) -> read bits
      // First idenitfy if the connected panel is 8 or 16 Zone. To do this we have to count the clock cycles: 24 -> 8 Zone, 40 -> 16 Zone
      // real work happens here
      auto now_ms = millis();
      if (clock_bit) {
        // on rising edge
        // data read happens here
        // bool data_bit = arg->pin_data_.digital_read();
        arg->bits_received++;
        arg->packet_bits++;
        if (arg->processor_.decode(now_ms, data_bit, arg->frame_size)) {
          arg->last_packet_ms = now_ms;
          arg->packets_received++;
          if (arg->packet_interrupts > arg->packet_bits) {
            arg->packets_with_interference++;
          }
          arg->packet_interrupts = 0;
          arg->packet_bits = 0;
          arg->set_data_(arg->processor_.packet);
        }
      }
      else {
        // on falling edge
        arg->last_interrupt_us_ = now_us;
        // check if new rame started
        arg->processor_.next(now_ms);
        // write command
        arg->processor_.write(&arg->pin_data_out_);
      }
      // siren handling
      //bool data_bit = arg->pin_siren_.digital_read();
      data_bit = arg->pin_siren_.digital_read();
      if (data_bit) {
        arg->siren = 0x0001;
      }
      else {
        arg->siren = 0x0000;
      }
    }
*/

    void IRAM_ATTR MicronStore::set_data_(MicronPacket *packet) {
      this->command = packet->command;
      this->status = packet->status;
    }

    uint8_t MicronComponent::last_command() {
      return this->last_command_;
    }

    void MicronComponent::write(uint8_t command) {
      // ESP_LOGD(TAG, "Enqueue command: 0x%02x", command);
      this->command_queue_.push(command);
    }

    void MicronComponent::write(std::vector<uint8_t> commands) {
      for (const auto command : commands)
      {
          this->write(command);
      }
    }

    void MicronComponent::press(const std::string &keys) {
      ESP_LOGD(TAG, "Press keys: %s", keys.c_str());
      this->write(keys_to_commands(keys));
    }

    void MicronComponent::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up Micron...");

      //this->store_.setup(this->pin_clock_, this->pin_data_, this->pin_data_out_);
      // starting with board ID
      this->store_.setup(this->pin_clock_, this->pin_clock2_, this->pin_data_, this->pin_data_out_, this->pin_siren_, this->pin_siren_out_);
      ESP_LOGCONFIG(TAG, "Setting up Micron...COMPLETED");
    }

    void MicronComponent::dump_config() {
      //LOG_SENSOR("", "Micron", this);
      ESP_LOGCONFIG(TAG, "Micron:");
      LOG_PIN("  Pin Clock: ", this->pin_clock_);
      LOG_PIN("  Pin Data: ", this->pin_data_);
      LOG_PIN("  Pin Data Out: ", this->pin_data_out_);
      LOG_PIN("  Siren Data: ", this->pin_siren_);
      LOG_PIN("  Siren Data Out: ", this->pin_siren_out_);
      LOG_BINARY_SENSOR("  ", "Zone 1", this->zone1_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 2", this->zone2_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 3", this->zone3_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 4", this->zone4_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 5", this->zone5_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 6", this->zone6_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 7", this->zone7_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Zone 8", this->zone8_binary_sensor_);
    }

    void MicronComponent::loop() {
      ESP_LOGCONFIG(TAG, "Waiting for board ID");
      if (this->store_.alarm_board_type == MICRON_TYPE_UNKNOWN) {
        ESP_LOGCONFIG(TAG, "Board not yet ID....");
        // setup falling edge interrupt
        this->store_.setupFall(this->pin_clock_);
      }
      else {
        //ESP_LOGCONFIG(TAG, "Board ID!");

        bool is_connected = (millis() - this->store_.last_packet_ms) < MICRON_CLOCK_TIMEOUT_MS;
        if (!is_connected) {
          this->store_.status = 0x00;
        }
        if (this->m_binary_sensor_) {
          this->m_binary_sensor_->publish_state((this->store_.status & MICRON_M_MASK) == MICRON_M_MASK);
        }
        if (this->battery_binary_sensor_) {
          this->battery_binary_sensor_->publish_state((this->store_.status & MICRON_BATTERY_MASK) == MICRON_BATTERY_MASK);
        }
        if (this->zonea_binary_sensor_) {
          this->zonea_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_A_MASK) == MICRON_ZONE_A_MASK);
        }
        if (this->zoneb_binary_sensor_) {
          this->zoneb_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_B_MASK) == MICRON_ZONE_B_MASK);
        }

        if (this->beep1_binary_sensor_) {
          this->beep1_binary_sensor_->publish_state((this->store_.status & MICRON_KEY_BEEP_1_MASK) == MICRON_KEY_BEEP_1_MASK);
        }
        if (this->beep3_binary_sensor_) {
          this->beep3_binary_sensor_->publish_state((this->store_.status & MICRON_KEY_BEEP_3_MASK) == MICRON_KEY_BEEP_3_MASK);
        }

        if (this->zone1_binary_sensor_) {
          this->zone1_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_1_MASK) == MICRON_ZONE_1_MASK);
        }
        if (this->zone2_binary_sensor_) {
          this->zone2_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_2_MASK) == MICRON_ZONE_2_MASK);
        }
        if (this->zone3_binary_sensor_) {
          this->zone3_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_3_MASK) == MICRON_ZONE_3_MASK);
        }
        if (this->zone4_binary_sensor_) {
          this->zone4_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_4_MASK) == MICRON_ZONE_4_MASK);
        }
        if (this->zone5_binary_sensor_) {
          this->zone5_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_5_MASK) == MICRON_ZONE_5_MASK);
        }
        if (this->zone6_binary_sensor_) {
          this->zone6_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_6_MASK) == MICRON_ZONE_6_MASK);
        }
        if (this->zone7_binary_sensor_) {
          this->zone7_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_7_MASK) == MICRON_ZONE_7_MASK);
        }
        if (this->zone8_binary_sensor_) {
          this->zone8_binary_sensor_->publish_state((this->store_.status & MICRON_ZONE_8_MASK) == MICRON_ZONE_8_MASK);
        }

        if (this->keypad_text_sensor_ && this->command_dedupe_.next(this->store_.command) && this->store_.command != 0x00) {
          this->keypad_text_sensor_->publish_state(str_sprintf("0x%02x", this->store_.command));
        }
        if (this->status_text_sensor_ && this->status_dedupe_.next(this->store_.status)) {
          this->status_text_sensor_->publish_state(str_sprintf("0x%04x", this->store_.status));
        }
        if (this->siren_binary_sensor_) {
          this->siren_binary_sensor_->publish_state((this->store_.siren & MICRON_SIREN_MASK) == MICRON_SIREN_MASK);
        }
        if (this->connected_binary_sensor_) {
          this->connected_binary_sensor_->publish_state(is_connected);
        }

        if (this->night_binary_sensor_) {
          this->night_binary_sensor_->publish_state((this->store_.status & MICRON_NIGHT_ARMED_MASK) == MICRON_NIGHT_ARMED_MASK);
        }
        if (this->test2_binary_sensor_) {
          this->test2_binary_sensor_->publish_state((this->store_.status & MICRON_0800_MASK) == MICRON_0800_MASK);
        }

        if (!this->command_queue_.empty() && (millis() - this->last_command_ms_) >= MICRON_MAX_COMMAND_DELAY_MS) {
          this->last_command_ms_ = millis();
          auto command = this->command_queue_.front();
          ESP_LOGD(TAG, "Write command: 0x%02x", command);
          this->store_.write(command, 2);
          this->last_command_ = command;
          this->command_queue_.pop();
          if (this->command_queue_.empty()) {
            ESP_LOGD(TAG, "All commands written");
          }
        }
      }
    }

    void MicronComponent::update() {
      ESP_LOGD(TAG, "Command: 0x%02x,  Status: 0x%08x", this->store_.command, this->store_.status);
      ESP_LOGD(TAG, "Interrupts: %d, Bits: %d, Packets: %d, Packets Fixed: %d, Commands Sent: %d",
        this->store_.interrupts,
        this->store_.bits_received,
        this->store_.packets_received,
        this->store_.packets_with_interference,
        this->store_.commands_sent);
      ESP_LOGD(TAG, "Board. %d, Clock: %d, Cycle: %d",
        this->store_.alarm_board_type,
        this->store_.id_clock_count,
        this->store_.id_cycle_count);
      //ESP_LOGD(TAG, "Write stats: 0x%02x  repeat %d bits %d", this->store_.processor_.command_out, this->store_.processor_.command_repeat, this->store_.processor_.remaining_command_writes);
    }

    float MicronComponent::get_setup_priority() const { return setup_priority::DATA; }

  } // namespace micron
} // namespace esphome
