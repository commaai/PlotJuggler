#include "rlog_parser.hpp"

bool rlogMessageParser::parseMessage(const MessageRef msg, double time_stamp){
	return false;
}

bool rlogMessageParser::parseMessageImpl(const std::string& topic_name, capnp::DynamicValue::Reader value, double time_stamp){

  PJ::PlotData& _data_series = getSeries(topic_name);
	
  switch (value.getType()) {
    case capnp::DynamicValue::BOOL:
      _data_series.pushBack({time_stamp, value.as<bool>()});
      break;

    case capnp::DynamicValue::INT:
      _data_series.pushBack({time_stamp, value.as<int64_t>()});
      break;

    case capnp::DynamicValue::UINT:
      _data_series.pushBack({time_stamp, value.as<uint64_t>()});
      break;

    case capnp::DynamicValue::FLOAT:
      _data_series.pushBack({time_stamp, value.as<double>()});
      break;

    case capnp::DynamicValue::LIST: {
	// Skipping lists for now
	// TODO: think of how to plot lists
	break;
    }

    case capnp::DynamicValue::ENUM: {
	// TODO Fix ENUM
        //auto enumValue = value.as<capnp::DynamicEnum>();

	/*
        KJ_IF_MAYBE(enumerant, enumValue.getEnumerant()) {
        std::cout <<
        enumerant->getProto().getName().cStr();
        }
        else {
        // Unknown enum value; output raw number.
        }
	*/
        //_data_series.pushBack({time_stamp, enumValue.getRaw()});
      break;
    }

    case capnp::DynamicValue::STRUCT: {
      auto structValue = value.as<capnp::DynamicStruct>();
      bool first = true;

      for (auto field: structValue.getSchema().getFields()) {

        if (!structValue.has(field))
		continue;

	std::string name =  field.getProto().getName().cStr();
        parseMessageImpl(topic_name + '/' + name, structValue.get(field), time_stamp); 
      }

      break;
    }
    default:
      // There are other types, we aren't handling them.
      break;
  }

  return true;
}


