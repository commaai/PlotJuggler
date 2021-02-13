#include "dataload_rlog.hpp"
#include "rlog_parser.hpp"
#include <iostream>

/*
======================================== 
	DataLoadrlog Functions:
======================================== 
*/

DataLoadrlog::DataLoadrlog(){
  _extensions.push_back("bz2");
}

DataLoadrlog::~DataLoadrlog(){
}

const std::vector<const char*>& DataLoadrlog::compatibleFileExtensions() const{
  return _extensions;
}


bool DataLoadrlog::readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){
  
  Events events;
  QReadWriteLock events_lock;
  QMap<int, QPair<int, int> >eidx;
  
  auto fn = fileload_info->filename;
  qDebug() << "Loading: " << fn;
  LogReader* log_reader = new LogReader(fn, &events, &events_lock, &eidx);
  
  QThread* thread = new QThread;
  log_reader->moveToThread(thread);
  QObject::connect(thread, SIGNAL (started()), log_reader, SLOT (process()));
  //QObject::connect(log_reader, SIGNAL (done()), thread, SLOT (quit()));
  thread->start();
  
  // temporary hack for waiting for events to load
  sleep(3);
  
  QList<uint64_t> times = events.uniqueKeys();
  rlogMessageParser parser("", plot_data);
  
  std::vector<uint8_t> buffer;
  
  std::cout << "Parsing..." << std::endl;
  int error_count = 0;
  
  for(auto time : times){

    QList<capnp::DynamicStruct::Reader> event = events.values(time);

    // Read in time and event
    for(auto val : event){
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
  }

  std::cout << "Done parsing" << std::endl;
  if (error_count) {
    std::cout << error_count << " messages failed to parse" << std::endl;
  }

  return true;
}

bool DataLoadrlog::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const{
  return false;
}

bool DataLoadrlog::xmlLoadState(const QDomElement& parent_element){
  return false;
}
