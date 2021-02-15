#include "dataload_rlog.hpp"
#include "rlog_parser.hpp"
#include <iostream>

/*
======================================== 
	DataLoadRLog Functions:
======================================== 
*/

DataLoadRLog::DataLoadRLog(){
  _extensions.push_back("bz2"); }

DataLoadRLog::~DataLoadRLog(){
}

const std::vector<const char*>& DataLoadRLog::compatibleFileExtensions() const{
  return _extensions;
}

bool DataLoadRLog::readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){
  
  Events events;
  
  auto fn = fileload_info->filename;
  qDebug() << "Loading: " << fn;

  // Load file:
  bz_stream bStream;
  bStream.next_in = NULL;
  bStream.avail_in = 0;
  bStream.bzalloc = NULL;
  bStream.bzfree = NULL;
  bStream.opaque = NULL;

  int ret = BZ2_bzDecompressInit(&bStream, 0, 0);
  if (ret != BZ_OK) qWarning() << "bz2 init failed";

  QByteArray raw;
  // 64MB buffer
  raw.resize(1024*1024*64);

  // auto increment?
  bStream.next_out = raw.data();
  bStream.avail_out = raw.size();

  int event_offset = 0;

  QFile* loaded_file = new QFile(fn);
  loaded_file->open(QIODevice::ReadOnly);
  QByteArray dat = loaded_file->readAll();
  loaded_file->close();

  bStream.next_in = dat.data();
  bStream.avail_in = dat.size();

  while(bStream.avail_in > 0){
    int ret = BZ2_bzDecompress(&bStream);
    if(ret != BZ_OK && ret != BZ_STREAM_END){
      qWarning() << "bz2 decompress failed";
      break;
    }
    qDebug() << "got" << dat.size() << "with" << bStream.avail_out << "size" << raw.size();
  }

  int dled = raw.size() - bStream.avail_out;

  auto amsg = kj::arrayPtr((const capnp::word*)(raw.data() + event_offset), (dled-event_offset)/sizeof(capnp::word));

  //Parse the schema:
  auto fs = kj::newDiskFilesystem();
  capnp::SchemaParser schema_parser;

  auto fs_imp = kj::newDiskFilesystem();
  auto import = fs_imp->getRoot().openSubdir(kj::Path::parse("home/batman/openpilot/cereal"));

  // TODO: improve this
  // ---
  kj::ArrayBuilder<const kj::ReadableDirectory* const> builder = kj::heapArrayBuilder<const kj::ReadableDirectory* const>(1);
  builder.add(import);

  auto importDirs = builder.finish().asPtr();
  // ---

  capnp::ParsedSchema schema = schema_parser.parseFromDirectory(fs->getRoot(), kj::Path::parse("home/batman/openpilot/cereal/log.capnp"), importDirs);

  capnp::ParsedSchema evnt = schema.getNested("Event");
  capnp::StructSchema evnt_struct = evnt.asStruct();

  while(amsg.size() > 0){
    // Get events
    try
    {
      capnp::FlatArrayMessageReader cmsg = capnp::FlatArrayMessageReader(amsg);

      // this needed? it is
      capnp::FlatArrayMessageReader *tmsg = new capnp::FlatArrayMessageReader(kj::arrayPtr(amsg.begin(), cmsg.getEnd()));
      amsg = kj::arrayPtr(cmsg.getEnd(), amsg.end());

      capnp::DynamicStruct::Reader event_example = tmsg->getRoot<capnp::DynamicStruct>(evnt_struct);

      auto logMonoTime = event_example.get("logMonoTime").as<uint64_t>();

      events.insert(logMonoTime, event_example);

      // increment
      event_offset = (char*)cmsg.getEnd() - raw.data();

    }
    catch (const kj::Exception& e)
    {
      break;
    }
  }

  printf("parsed %d into %d events with offset %d\n", dled, events.size(), event_offset);
  

  // Parse event:
  QList<uint64_t> times = events.uniqueKeys();
  RLogMessageParser parser("", plot_data);
  
  std::cout << "Parsing..." << std::endl;
  int error_count = 0;
  
  for(auto time : times){

    capnp::DynamicStruct::Reader val = events.value(time);

    try
    {
      parser.parseMessageImpl("", val, (double)time / 1e9);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error parsing message. logMonoTime: " << val.get("logMonoTime").as<uint64_t>() << std::endl;
        std::cerr << e.what() << std::endl;
        error_count++;
        continue;
    }
  }

  std::cout << "Done parsing" << std::endl;
  if (error_count) {
    std::cout << error_count << " messages failed to parse" << std::endl;
  }

  return true;
}

bool DataLoadRLog::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const{
  return false;
}

bool DataLoadRLog::xmlLoadState(const QDomElement& parent_element){
  return false;
}
