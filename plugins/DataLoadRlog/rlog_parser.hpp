#pragma once

#include <PlotJuggler/plotdata.h>
#include <PlotJuggler/messageparser_base.h>
#include <capnp/dynamic.h>

using namespace PJ;

class RlogMessageParser : MessageParser
{

public:
  RlogMessageParser(const std::string& topic_name, PJ::PlotDataMapRef& plot_data):
    MessageParser(topic_name, plot_data) {}

  bool parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader node, double timestamp);

  bool parseMessage(const MessageRef serialized_msg, double timestamp);
};
