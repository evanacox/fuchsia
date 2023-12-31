// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICES_POWER_DRIVERS_FUSB302_USB_PD_H_
#define SRC_DEVICES_POWER_DRIVERS_FUSB302_USB_PD_H_

#include <hwreg/bitfields.h>

// This file contains some of the message structures defined by the USB Power Delivery
// Specification. For details on the fields or to find more fields/message types not included in
// this file please read the specs.

namespace usb {
namespace pd {

static constexpr size_t kMaxObjects = 7;
static constexpr size_t kObjectSize = sizeof(uint32_t);
static constexpr size_t kMaxLen = kMaxObjects * kObjectSize;

// Specification Revision
enum SpecRev : uint8_t {
  kRev1 = 0b00,
  kRev2 = 0b01,
  kRev3 = 0b10,
};

struct Header {
  uint16_t value;

  // extended: indicates whether the following message is of type PdMessageType::EXTENDED.
  DEF_SUBBIT(value, 15, extended);

  // num_data_objects: number of data objects (32-bit) following header in payload.
  DEF_SUBFIELD(value, 14, 12, num_data_objects);

  // message_id: message_id generated by rolling counter of message origin.
  DEF_SUBFIELD(value, 11, 9, message_id);

  // port_power_role_or_cable_plug:
  //    For SOP messages: port_power_role--0 is sink, 1 is source.
  //    For SOP'/SOP" messages: cable_plug--0 is Message originated from a DFP or UFP, 1 is Message
  //                           originated from a Cable Plug or VPD.
  DEF_SUBBIT(value, 8, port_power_role_or_cable_plug);

  // spec_rev: Specification Revision.
  DEF_ENUM_SUBFIELD(value, SpecRev, 7, 6, spec_rev);

  // port_data_role_or_reserved:
  //    For SOP messages: port_data_role--0 is UFP, 1 is DFP.
  //    For SOP'/SOP" messages: reserved.
  DEF_SUBBIT(value, 5, port_data_role_or_reserved);

  // message_type: sub message type as defined by each PdMessageType, ex: ControlMessageType,
  // DataMessageType, ExtendedMessageType.
  DEF_SUBFIELD(value, 4, 0, message_type);

  explicit Header(uint16_t val) : value(val) {}
  Header(bool extended, uint8_t num_data_objects, uint8_t message_id,
         bool port_power_role_or_cable_plug, SpecRev spec_rev, bool port_data_role_or_reserved,
         uint8_t message_type) {
    set_extended(extended);
    set_num_data_objects(num_data_objects);
    set_message_id(message_id % 8);
    set_port_power_role_or_cable_plug(port_power_role_or_cable_plug);
    set_spec_rev(spec_rev);
    set_port_data_role_or_reserved(port_data_role_or_reserved);
    set_message_type(message_type);
  }
};

// Base Power Delivery Message: each power delivery message can have one of 3 types (PdMessageType):
// CONTROL, DATA, EXTENDED. Defined by various bits in the header.
class PdMessage {
 public:
  enum PdMessageType {
    CONTROL,
    DATA,
    EXTENDED,

    NONE,
  };

  PdMessage(const PdMessage& message) : header_(message.header().value) {
    payload_ = message.payload();
  }
  PdMessage(uint16_t header, const uint8_t* payload) : header_(header) {
    if (payload) {
      memcpy(payload_.data(), payload, header_.num_data_objects() * kObjectSize);
    }
  }
  PdMessage(bool extended, uint8_t num_data_objects, uint8_t message_id,
            bool port_power_role_or_cable_plug, SpecRev spec_rev, bool port_data_role_or_reserved,
            uint8_t message_type, const uint8_t* payload)
      : header_(extended, num_data_objects, message_id, port_power_role_or_cable_plug, spec_rev,
                port_data_role_or_reserved, message_type) {
    if (payload) {
      memcpy(payload_.data(), payload, (header_.num_data_objects() & 0x7) * kObjectSize);
    }
  }
  PdMessage& operator=(const PdMessage& other) {
    if (this == &other) {
      return *this;
    }

    header_ = Header(other.header().value);
    payload_ = other.payload();
    return *this;
  }

  PdMessageType GetPdMessageType() const {
    return header().extended() ? EXTENDED : (header().num_data_objects() ? DATA : CONTROL);
  }

  const Header& header() const { return header_; }
  std::array<uint8_t, kMaxLen>& payload() { return payload_; }
  const std::array<uint8_t, kMaxLen>& payload() const { return payload_; }

 private:
  // header_: a 32-bit value with bits as defined in Header.
  Header header_;
  // payload_: a list of 8-bit values (in little endian order), whose meaning is defined by the
  // specific power delivery message type and subsequent message types, which are defined in their
  // classes.
  std::array<uint8_t, kMaxLen> payload_ = {0};
};

// Control Power Delivery Message: a type of PdMessage. This class helps define some bits in the
// header for convenience and defines ControlMessageTypes to be used in the message_type field in
// header.
class ControlPdMessage : public PdMessage {
 public:
  enum ControlMessageType : uint8_t {
    GOOD_CRC = 0b00001,
    GOTO_MIN = 0b00010,
    ACCEPT = 0b00011,
    REJECT = 0b00100,
    PING = 0b00101,
    PS_RDY = 0b00110,
    GET_SOURCE_CAP = 0b00111,
    GET_SINK_CAP = 0b01000,
    DR_SWAP = 0b01001,
    PR_SWAP = 0b01010,
    VCONN_SWAP = 0b01011,
    WAIT = 0b01100,
    SOFT_RESET = 0b01101,
    // Only available for kRev > 2.0
    DATA_RESET = 0b01110,
    DATA_RESET_COMPLETE = 0b01111,
    NOT_SUPPORTED = 0b10000,
    GET_SOURCE_CAP_EXTENDED = 0b10001,
    GET_STATUS = 0b10010,
    FR_SWAP = 0b10011,
    GET_PPS_STATUS = 0b10100,
    GET_COUNTRY_CODES = 0b10101,
    GET_SINK_CAP_EXTENDED = 0b10110,
  };

  ControlPdMessage(uint8_t message_id, bool port_power_role_or_cable_plug, SpecRev spec_rev,
                   bool port_data_role_or_reserved, ControlMessageType message_type)
      : PdMessage(/* extended */ false, /* num_data_objects */ 0, message_id,
                  port_power_role_or_cable_plug, spec_rev, port_data_role_or_reserved, message_type,
                  /* payload */ nullptr) {}
};

// Data Power Delivery Message: a type of PdMessage. This class helps define some bits in the
// header for convenience and defines DataMessageTypes to be used in the message_type field in
// header. DataPdMessage also defines some of the structs to be used by the payload.
class DataPdMessage : public PdMessage {
 public:
  enum DataMessageType : uint8_t {
    SOURCE_CAPABLILITIES = 0b00001,
    REQUEST = 0b00010,
    BIST = 0b00011,
    SINK_CAPABILITIES = 0b00100,
    BATTERY_STATUS = 0b00101,
    ALERT = 0b00110,
    GET_COUNTRY_INFO = 0b00111,
    ENTER_USB = 0b01000,
    VENDOR_DEFINED = 0b01111,
  };

  DataPdMessage(uint8_t num_data_objects, uint8_t message_id, bool port_power_role_or_cable_plug,
                SpecRev spec_rev, bool port_data_role_or_reserved, DataMessageType message_type,
                uint8_t* payload)
      : PdMessage(/* extended */ false, num_data_objects, message_id, port_power_role_or_cable_plug,
                  spec_rev, port_data_role_or_reserved, message_type, payload) {}

  // Power supply type
  enum PowerType : uint8_t {
    FIXED_SUPPLY = 0b00,
    BATTERY = 0b01,
    VARIABLE_SUPPLY = 0b10,
    AUGMENTED_POWER = 0b11,
  };

  // PowerDataObject: base power data object (PDO) with 32-bits. Each PDO can be one of 4 power
  // supply types as defined in PowerType, indicated by the power_type field. Remaining bits are
  // defined by which power type the PDO is describing as described by the following structs.
  struct PowerDataObject {
    // value: 32-bit value that the PDO represents.
    uint32_t value;

    // power_type: power supply type defined in PowerType.
    DEF_ENUM_SUBFIELD(value, PowerType, 31, 30, power_type);

    explicit PowerDataObject(uint32_t val) : value(val) {}
  };

  // Fixed Supply PDO: a type of PowerDataObject. This class define the rest of the bits in the
  // 32-bit value for fixed supplies.
  struct FixedSupplyPDO : PowerDataObject {
    // dual_role_power: set if the port is Dual-Role Power capable.
    DEF_SUBBIT(value, 29, dual_role_power);

    // usb_suspend_supported: set if the Sink Shall follow the [USB 2.0] or [USB 3.2] rules for
    // suspend and resume.
    DEF_SUBBIT(value, 28, usb_suspend_supported);

    // unconstrained_power: set when an external source of power is available that is sufficient to
    // adequately power the system while charging external devices, or when the device’s primary
    // function is to charge external devices.
    DEF_SUBBIT(value, 27, unconstrained_power);

    // usb_communications_capable: set for Sources capable of communication over the USB data lines.
    DEF_SUBBIT(value, 26, usb_communications_capable);

    // dual_role_data: set when the Port is Dual-Role data capable.
    DEF_SUBBIT(value, 25, dual_role_data);

    // unchunked_extended_messages_supported: set when the Port can send and receive Extended
    // Messages with Data Size > MaxExtendedMsgLegacyLen bytes in a single, Unchunked Message.
    DEF_SUBBIT(value, 24, unchunked_extended_messages_supported);

    // peak_current: peak current for overload capabilities
    DEF_SUBFIELD(value, 21, 20, peak_current);

    // voltage: voltage offered by fixed supply in 50 mV units.
    DEF_SUBFIELD(value, 19, 10, voltage_50mV);

    // maximum_current_10mA: maximum current offered by fixed supply in 10 mA units.
    DEF_SUBFIELD(value, 9, 0, maximum_current_10mA);

    explicit FixedSupplyPDO(uint32_t val) : PowerDataObject(val) {}
  };

  // Battery Supply PDO: a type of PowerDataObject. This class define the rest of the bits in the
  // 32-bit value for battery supplies.
  struct BatterySupplyPDO : PowerDataObject {
    // maximum_voltage_50mV: maximum voltage offered by battery in 50 mV units.
    DEF_SUBFIELD(value, 29, 20, maximum_voltage_50mV);

    // minimum_voltage_50mV: minimum voltage offered by battery in 50 mV units.
    DEF_SUBFIELD(value, 19, 10, minimum_voltage_50mV);

    // maximum_power_250mW: maximum power offered by battery in 250 mW units.
    DEF_SUBFIELD(value, 9, 0, maximum_power_250mW);

    explicit BatterySupplyPDO(uint32_t val) : PowerDataObject(val) {}
  };

  // Variable Supply PDO: a type of PowerDataObject. This class define the rest of the bits in the
  // 32-bit value for variable supplies.
  struct VariableSupplyPDO : PowerDataObject {
    // maximum_voltage_50mV: maximum voltage offered by variable supply in 50 mV units.
    DEF_SUBFIELD(value, 29, 20, maximum_voltage_50mV);

    // minimum_voltage_50mV: minimum voltage offered by variable supply in 50 mV units.
    DEF_SUBFIELD(value, 19, 10, minimum_voltage_50mV);

    // maximum_current_10mA: maximum current offered by variable supply in 10 mA units.
    DEF_SUBFIELD(value, 9, 0, maximum_current_10mA);

    explicit VariableSupplyPDO(uint32_t val) : PowerDataObject(val) {}
  };

  // Only programmable power supply supported for augmented power
  // Programmable Power Supply PDO: a type of PowerDataObject. This class define the rest of the
  // bits in the 32-bit value for programmable power supplies.
  struct ProgrammablePowerSupplyAPDO : PowerDataObject {
    enum AugmentedType : uint8_t {
      PROGRAMMABLE = 0b00,
    };
    // augmented_type: Augmented power data objects can have multiple types (but only 1 is defined
    // for now). This field indicates which type (as defined by AugmentedType).
    DEF_ENUM_SUBFIELD(value, AugmentedType, 29, 28, augmented_type);

    // pps_power_limited: set to limit power supplied by source to source's rated PDP.
    DEF_SUBBIT(value, 27, pps_power_limited);

    // maximum_voltage_100mV: maximum voltage offered by programmable poewr supply in 100 mV units.
    DEF_SUBFIELD(value, 24, 17, maximum_voltage_100mV);

    // minimum_voltage_100mV: minimum voltage offered by programmable poewr supply in 100 mV units.
    DEF_SUBFIELD(value, 15, 8, minimum_voltage_100mV);

    // maximum_current_50mA: maximum current offered by programmable poewr supply in 50 mA units.
    DEF_SUBFIELD(value, 6, 0, maximum_current_50mA);

    explicit ProgrammablePowerSupplyAPDO(uint32_t val) : PowerDataObject(val) {}
  };

  // RequestDataObject: base request data object (RDO) with 32-bits. Each RDO can be for one of 4
  // power supply types as defined in PowerType. This struct defines the common bits for each RDO,
  // bits 0-20 are defined for each power type.
  struct RequestDataObject {
    // value: 32-bit value that the PDO represents.
    uint32_t value;

    // object_position: indicatewswhich object in the Source_Capabilities Message the RDO refers.
    DEF_SUBFIELD(value, 30, 28, object_position);  // 0b000 is reserved

    // give_back: set to indicate that the Sink will respond to a GotoMin Message by reducing its
    // load to the Minimum Operating Current
    DEF_SUBBIT(value, 27, give_back);

    // capability_mismatch: set when the Sink cannot satisfy its power requirements from the
    // capabilities offered by the Source.
    DEF_SUBBIT(value, 26, capability_mismatch);

    // usb_communications_capable: set to one when the Sink has USB data lines and is capable of
    // communicating using either [USB 2.0] or [USB 3.2] protocols
    DEF_SUBBIT(value, 25, usb_communications_capable);

    // no_usb_suspend: set by the Sink to indicate to the Source that this device is requesting to
    // continue its Contract during USB Suspend
    DEF_SUBBIT(value, 24, no_usb_suspend);

    // unchunked_extended_messages_supported: set when the Port can send and receive Extended
    // Messages with Data Size > MaxExtendedMsgLegacyLen bytes in a single, Unchunked Message
    DEF_SUBBIT(value, 23, unchunked_extended_messages_supported);

    explicit RequestDataObject(uint32_t val) : value(val) {}
  };

  // Fixed (and Variable) Supply RDO: a type of RequestDataObject. This class define the rest of the
  // bits in the 32-bit value for fixed and variable supplies.
  struct FixedVariableSupplyRDO : RequestDataObject {
    // operating_current_10mA: set to the actual amount of current the Sink needs to operate at a
    // given time in 10 mA units.
    DEF_SUBFIELD(value, 19, 10, operating_current_10mA);

    // maximum_current_10mA: set to the highest current the Sink will ever require in 10 mA units.
    DEF_SUBFIELD(value, 9, 0, maximum_current_10mA);

    explicit FixedVariableSupplyRDO(uint32_t val) : RequestDataObject(val) {}
  };

  // Battery Supply RDO: a type of RequestDataObject. This class define the rest of the  bits in the
  // 32-bit value for battery supplies.
  struct BatterySupplyRDO : RequestDataObject {
    // operating_power_250mW: set to the actual amount of power the Sink wants at this time in 250
    // mW units.
    DEF_SUBFIELD(value, 19, 10, operating_power_250mW);

    // maximum_operating_power_250mW: set to the highest power the Sink will ever require in 250 mW
    // units.
    DEF_SUBFIELD(value, 9, 0, maximum_operating_power_250mW);

    explicit BatterySupplyRDO(uint32_t val) : RequestDataObject(val) {}
  };

  // Progremmable Power Supply RDO: a type of RequestDataObject. This class define the rest of the
  // bits in the 32-bit value for programmable power supplies.
  struct ProgrammablePowerSupplyRDO : RequestDataObject {
    // output_voltage_20mV: set by the Sink to the voltage the Sink requires as measured at the
    // Source’s output connector in 20 mV units.
    DEF_SUBFIELD(value, 19, 9, output_voltage_20mV);

    // operating_current_50mA: set to the actual amount of current the Sink needs to operate at a
    // given time in 50 mA units.
    DEF_SUBFIELD(value, 6, 0, operating_current_50mA);

    explicit ProgrammablePowerSupplyRDO(uint32_t val) : RequestDataObject(val) {}
  };
};

// Extended Power Delivery Message: a type of PdMessage. This class helps define some bits in the
// header for convenience and defines ExtendedMessageTypes to be used in the message_type field in
// header.
class ExtendedPdMessage : public PdMessage {
 public:
  enum ExtendedMessageType : uint8_t {
    SOURCE_CAPABLILITES_EXTENDED = 0b00001,
    STATUS = 0b00010,
    GET_BATTERY_CAP = 0b00011,
    GET_BATTERY_STATUS = 0b00100,
    BATTERY_CAPABILITIES = 0b00101,
    GET_MANUFACTURER_INFO = 0b00110,
    MANUFACTURER_INFO = 0b00111,
    SECURITY_REQUEST = 0b01000,
    SECURITY_RESPONSE = 0b01001,
    FIRMWARE_UPDATE_REQUEST = 0b01010,
    FIRMWARE_UPDATE_RESPONSE = 0b01011,
    PPS_STATUS = 0b01100,
    COUNTRY_INFO = 0b01101,
    COUNTRY_CODES = 0b01110,
    SINK_CAPABILITIES_EXTENDED = 0b01111,
  };

  ExtendedPdMessage(uint8_t num_data_objects, uint8_t message_id,
                    bool port_power_role_or_cable_plug, SpecRev spec_rev,
                    bool port_data_role_or_reserved, ExtendedMessageType message_type,
                    uint8_t* payload)
      : PdMessage(/* extended */ true, num_data_objects, message_id, port_power_role_or_cable_plug,
                  spec_rev, port_data_role_or_reserved, message_type, payload) {}
};

}  // namespace pd
}  // namespace usb

#endif  // SRC_DEVICES_POWER_DRIVERS_FUSB302_USB_PD_H_
