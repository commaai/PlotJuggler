#include "dataload_rlog.h"
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

	/*
	 //Url stuff:
	std::string urls_file = fileload_info->filename.toStdString();
	std::ifstream infile;
	infile.open(urls_file);

	std::string line;
	std::getline(infile, line);

	std::string url = "http://data.comma.life/" + line + "/0/rlog.bz2";

	const QString example_url = QString::fromStdString(url);

	infile.close();
	*/

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

	//PlotDataAny& plot_consecutive = plot_data.addUserDefined("__consecutive_message_instances__")->second;

	std::vector<uint8_t> buffer;

	std::cout << "Parsing..." << std::endl;

	for(auto time : times){

		QList<capnp::DynamicStruct::Reader> event = events.values(time);
		capnp::DynamicStruct::Reader current_event;

		// Read in time and event
		for(auto val : event){
			if(val.has("initData") || val.has("sentinel")) // Skip
				continue;
			current_event = val;
		}

		/*
		auto data_point = PlotDataAny::Point(time, current_event);
		plot_consecutive.pushBack(data_point);

		auto plot_pair = plot_map.user_defined.find(topic_name);
		if(plot_pair == plot_map.user_defined.end()){
			plot_pair = plot_map.addUserDefined(topic_name);
		}

		PlotDataAny& plot_raw = plot_pair->second;
		plot_raw.pushBack(data_point);

		//----- skip not selected -----------
		if (topic_selected.find(topic_name) == topic_selected.end())
		{
		  continue;
		}


		const size_t msg_size = sizeof(current_event);
		buffer.resize(msg_size);
		MessageRef msg_serialized(buffer.data(), buffer.size());
		*/
		// parse

		parser.parseMessageImpl("", current_event, time);

	}

	std::cout << "Done parsing" << std::endl;

	return true;
}

bool DataLoadrlog::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const{
	return false;
}

bool DataLoadrlog::xmlLoadState(const QDomElement& parent_element){
	return false;
}

