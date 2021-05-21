#pragma once

#include <PlotJuggler/messageparser_base.h>
#include <capnp/dynamic.h>
#include <capnp/schema.h>
#include <capnp/serialize.h>
#include <iostream>
#include <QInputDialog>

#ifndef DYNAMIC_CAPNP
#define DYNAMIC_CAPNP  // Do not depend on generated log.capnp.h structure
#endif
#include "common.h"
#include "common_dbc.h"

using namespace PJ;

class RlogMessageParser : MessageParser
{
private:
  std::string dbc_name;
  std::unordered_map<uint8_t, std::shared_ptr<CANParser>> parsers;
  std::shared_ptr<CANPacker> packer;
  std::string SelectDBCDialog();
  bool loadDBC(std::string dbc_str);
  void initParser();
  bool show_deprecated;
  bool can_dialog_needed = true;
public:
  RlogMessageParser(const std::string& topic_name, PJ::PlotDataMapRef& plot_data):
    MessageParser(topic_name, plot_data) { initParser(); };

  bool parseMessageCereal(capnp::DynamicStruct::Reader event);
  bool parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader node, double timestamp, bool is_root);
  bool parseCanMessage(const std::string& topic_name, capnp::DynamicList::Reader node, double timestamp);
  bool parseMessage(const MessageRef serialized_msg, double timestamp);
  void showDBCDialog();
};
