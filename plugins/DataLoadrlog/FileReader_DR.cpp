#include "FileReader_DR.hpp"

#include <iostream>
#include <QtNetwork>

#include <capnp/schema-parser.h>
#include <capnp/schema-loader.h>
#include <kj/string.h>
#include <kj/filesystem.h>

using ::capnp::DynamicValue;
using ::capnp::DynamicStruct;
using ::capnp::DynamicEnum;
using ::capnp::DynamicList;
using ::capnp::List;
using ::capnp::Schema;
using ::capnp::StructSchema;
using ::capnp::EnumSchema;

using ::capnp::Void;
using ::capnp::Text;
using ::capnp::MallocMessageBuilder;
using ::capnp::PackedFdMessageReader;

void dynamicPrintValue(DynamicValue::Reader value) {
  // Print an arbitrary message via the dynamic API by
  // iterating over the schema.  Look at the handling
  // of STRUCT in particular.

  switch (value.getType()) {
    case DynamicValue::VOID:
      std::cout << "";
      std::cout << endl;
      break;

    case DynamicValue::BOOL:
      std::cout << (value.as<bool>() ? "true" : "false");
      std::cout << endl;
      break;

    case DynamicValue::INT:
      std::cout << value.as<int64_t>();
      std::cout << endl;
      break;

    case DynamicValue::UINT:
      std::cout << value.as<uint64_t>();
      std::cout << endl;
      break;

    case DynamicValue::FLOAT:
      std::cout << value.as<double>();
      std::cout << endl;
      break;

    case DynamicValue::TEXT:
      break;
      std::cout << '\"' << value.as<Text>().cStr() << '\"';
      std::cout << endl;

    case DynamicValue::LIST: {
      std::cout << "[";
      bool first = true;
      for (auto element: value.as<DynamicList>()) {
        if (first) {
          first = false;
        } else {
          std::cout << ", ";
        }
        dynamicPrintValue(element);
      }
      std::cout << "]";
      std::cout << endl;
      break;

    }
    /*
    case DynamicValue::ENUM: {
      auto enumValue = value.as<DynamicEnum>();
      KJ_IF_MAYBE(enumerant, enumValue.getEnumerant()) {
        std::cout <<
            enumerant->getProto().getName().cStr();
      } else {
        // Unknown enum value; output raw number.
        std::cout << enumValue.getRaw();
        std::cout << endl;

      }
      break;
    }
    */
    case DynamicValue::STRUCT: {
      std::cout << "(";
      auto structValue = value.as<DynamicStruct>();
      bool first = true;
      for (auto field: structValue.getSchema().getFields()) {
        if (!structValue.has(field)) continue;
        if (first) {
          first = false;
        } else {
          std::cout << ", ";
        }
        std::cout << field.getProto().getName().cStr()
                  << " = ";
        dynamicPrintValue(structValue.get(field));
      }
      std::cout << ")";
      std::cout << endl;

      break;
    }
    default:
      // There are other types, we aren't handling them.
      std::cout << "?";
      std::cout << endl;

      break;
  }
}

/* 
========================================
        FileReader Functions
========================================
*/



FileReader::FileReader(const QString& file_) : file(file_) {}

FileReader::~FileReader() {}

void FileReader::process() {
        loaded_file = new QFile(file);
	readyRead();
}
void FileReader::readyRead() {
	loaded_file->open(QIODevice::ReadOnly);
        QByteArray dat = loaded_file->readAll();
	loaded_file->close();
        printf("read: %d\n", dat.size());
}


/* 
========================================
        LogReader Functions
========================================
*/

LogReader::LogReader(const QString& file, Events* events_, QReadWriteLock* events_lock_, QMap<int, QPair<int, int> > *eidx_) : FileReader(file), events(events_), events_lock(events_lock_), eidx(eidx_) {

        bStream.next_in = NULL;
        bStream.avail_in = 0;
        bStream.bzalloc = NULL;
        bStream.bzfree = NULL;
        bStream.opaque = NULL;

        int ret = BZ2_bzDecompressInit(&bStream, 0, 0);
        if (ret != BZ_OK) qWarning() << "bz2 init failed";

        // 64MB buffer
        raw.resize(1024*1024*64);

        // auto increment?
        bStream.next_out = raw.data();
        bStream.avail_out = raw.size();

        event_offset = 0;

        parser = new std::thread([&]() {
                while (1) {
                        mergeEvents(cdled.get());
                }
        });

}

LogReader::~LogReader() {
  delete parser;
}

void LogReader::readyRead(){
	loaded_file->open(QIODevice::ReadOnly);
        QByteArray dat = loaded_file->readAll();
	loaded_file->close();
        bStream.next_in = dat.data();
        bStream.avail_in = dat.size();

        while (bStream.avail_in > 0) {
          int ret = BZ2_bzDecompress(&bStream);
          if (ret != BZ_OK && ret != BZ_STREAM_END) {
            qWarning() << "bz2 decompress failed";
            break;
          }
          qDebug() << "got" << dat.size() << "with" << bStream.avail_out << "size" << raw.size();
        }

        int dled = raw.size() - bStream.avail_out;
        cdled.put(dled);
}

void LogReader::mergeEvents(int dled){

        auto amsg = kj::arrayPtr((const capnp::word*)(raw.data() + event_offset), (dled-event_offset)/sizeof(capnp::word));
        Events* events_local = new Events;
        // QMap<int, QPair<int, int> > eidx_local;
	
	//Parse the schema:
	auto fs = kj::newDiskFilesystem();
	capnp::SchemaParser parser;

	auto fs_imp = kj::newDiskFilesystem();
	auto import = fs_imp->getRoot().openSubdir(kj::Path::parse("home/batman/openpilot/cereal"));

	// TODO: improve this
	// ---
	kj::ArrayBuilder<const kj::ReadableDirectory* const> builder = kj::heapArrayBuilder<const kj::ReadableDirectory* const>(1);
	builder.add(import);

	auto importDirs = builder.finish().asPtr();
	// ---
	
	capnp::ParsedSchema schema = parser.parseFromDirectory(fs->getRoot(), kj::Path::parse("home/batman/openpilot/cereal/log.capnp"), importDirs);

	capnp::ParsedSchema evnt = schema.getNested("Event");
	capnp::StructSchema evnt_struct = evnt.asStruct();

        while (amsg.size() > 0){
                // Get events
		try {

			capnp::FlatArrayMessageReader cmsg = capnp::FlatArrayMessageReader(amsg);

      			// this needed? it is
      			capnp::FlatArrayMessageReader *tmsg = new capnp::FlatArrayMessageReader(kj::arrayPtr(amsg.begin(), cmsg.getEnd()));
		        amsg = kj::arrayPtr(cmsg.getEnd(), amsg.end());

			capnp::DynamicStruct::Reader event_example = tmsg->getRoot<DynamicStruct>(evnt_struct);

			//dynamicPrintValue(event_example);
			
			uint64_t logMonoTime;	

			for(auto field : event_example.getSchema().getFields()){
				if(field.getProto().getName() == "logMonoTime"){
					logMonoTime = event_example.get(field).as<uint64_t>();
					break;
				}
			}

			//dynamicPrintValue(event_example);
			//printf("\n\n\n\n\n");

			events_local->insert(logMonoTime, event_example);

			// increment
			event_offset = (char*)cmsg.getEnd() - raw.data();

		}
		catch (const kj::Exception& e){
			break;
		}
        }



        events_lock->lockForWrite();
        *events += *events_local;
        //eidx->unite(eidx_local);
        events_lock->unlock();

        printf("parsed %d into %d events with offset %d\n", dled, events->size(), event_offset);
}

