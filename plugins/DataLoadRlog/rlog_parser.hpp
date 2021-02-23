#pragma once

#include <PlotJuggler/messageparser_base.h>
#include <capnp/dynamic.h>
#include <capnp/schema.h>
#include <capnp/serialize.h>
#include <iostream>

#ifndef DYNAMIC_CAPNP
#define DYNAMIC_CAPNP  // Do not depend on generated log.capnp.h structure
#endif
#include "../../3rdparty/opendbc/can/common.h"
#include "../../3rdparty/opendbc/can/common_dbc.h"

using namespace PJ;

class RlogMessageParser : MessageParser
{
private:
  CANParser* parser = nullptr;
  CANPacker* packer = nullptr;
public:
  RlogMessageParser(const std::string& topic_name, PJ::PlotDataMapRef& plot_data, std::string dbc_str);

  bool parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader node, double timestamp);
  bool parseCanMessage(const std::string& topic_name, capnp::DynamicList::Reader node, double timestamp);
  bool parseMessage(const MessageRef serialized_msg, double timestamp);
};
