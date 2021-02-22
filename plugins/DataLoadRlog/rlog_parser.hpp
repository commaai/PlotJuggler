#pragma once

#include <PlotJuggler/messageparser_base.h>
#include <capnp/dynamic.h>
#include <capnp/schema.h>
#include <capnp/serialize.h>
#include <iostream>

#include "common.h"
#include "cereal_gen_cpp_copy/log.capnp.h"

using namespace PJ;

class RlogMessageParser : MessageParser
{
private:
  CANParser* parser = nullptr;
  CANPacker* packer = nullptr;
public:
  RlogMessageParser(const std::string& topic_name, PJ::PlotDataMapRef& plot_data):
    MessageParser(topic_name, plot_data) {};

  bool parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader node, double timestamp);
  bool parseCanMessage(const std::string& topic_name, capnp::DynamicList::Reader node, double timestamp);
  bool parseMessage(const MessageRef serialized_msg, double timestamp);
};
