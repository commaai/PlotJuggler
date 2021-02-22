#include "rlog_parser.hpp"

bool RlogMessageParser::parseMessage(const MessageRef msg, double time_stamp)
{
  return false;
}

bool RlogMessageParser::parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader value, double time_stamp)
{

  PJ::PlotData& _data_series = getSeries(topic_name);

  switch (value.getType()) 
  {
    case capnp::DynamicValue::BOOL: 
    {
      _data_series.pushBack({time_stamp, (double)value.as<bool>()});
      break;
    }

    case capnp::DynamicValue::INT: 
    {
      _data_series.pushBack({time_stamp, (double)value.as<int64_t>()});
      break;
    }

    case capnp::DynamicValue::UINT: 
    {
      _data_series.pushBack({time_stamp, (double)value.as<uint64_t>()});
      break;
    }

    case capnp::DynamicValue::FLOAT: 
    {
      _data_series.pushBack({time_stamp, (double)value.as<double>()});
      break;
    }

    case capnp::DynamicValue::LIST: 
    {
      auto listValue = value.as<capnp::DynamicList>();
      if (topic_name.compare("/can") == 0) {
        parseCanMessage(topic_name, listValue, time_stamp);
      } else {
        // TODO: Parse lists properly
        int i = 0;
        for(auto element : listValue)
        {
          parseMessageImpl(topic_name + '/' + std::to_string(i), element, time_stamp);
          i++;
        }
      }
      break;
    }

    case capnp::DynamicValue::ENUM: 
    {
      auto enumValue = value.as<capnp::DynamicEnum>();
      _data_series.pushBack({time_stamp, (double)enumValue.getRaw()});
      break;
    }

    case capnp::DynamicValue::STRUCT: 
    {
      auto structValue = value.as<capnp::DynamicStruct>();
      for (auto field : structValue.getSchema().getFields()) 
      {
        if (structValue.has(field))
        {
          std::string name = field.getProto().getName();
          parseMessageImpl(topic_name + '/' + name, structValue.get(field), time_stamp);
        }
      }
      break;
    }

    default:
    {
      // We currently don't support: DATA, ANY_POINTER, TEXT, CAPABILITIES, VOID
      break;
    }
  }
  return true;
}

bool RlogMessageParser::parseCanMessage(
  const std::string& topic_name, capnp::DynamicList::Reader listValue, double time_stamp) 
{
  for(auto elem : listValue) {
    auto value = elem.as<capnp::DynamicStruct>();
    uint8_t bus = value.get("src").as<uint8_t>();
    if (bus == 1) {
      if (parser == nullptr) {
        parser = new CANParser(1, "honda_accord_s2t_2018_can_generated");
        packer = new CANPacker("honda_accord_s2t_2018_can_generated");
      }

      parser->UpdateCans((uint64_t)(time_stamp), value);
      for (auto& sg : parser->query_latest()) {
        PJ::PlotData& _data_series = getSeries(
          topic_name + '/' + packer->lookup_message(sg.address).name + '/' + sg.name);
        _data_series.pushBack({time_stamp, (double)sg.value});
      }
    }
  }
  return true;
}