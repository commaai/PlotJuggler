#include "dataload_rlog.h"
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

	QString example_file = "/home/batman/openpilot/tools/plots/PlotJuggler/datasamples/rlog.bz2";
	
	LogReader* log_reader = new LogReader(example_file, &events, &events_lock, &eidx);

	QThread* thread = new QThread;
        log_reader->moveToThread(thread);
        QObject::connect(thread, SIGNAL (started()), log_reader, SLOT (process()));
        //QObject::connect(log_reader, SIGNAL (done()), thread, SLOT (quit()));
        thread->start();

	// temporary hack for waiting for events to load
	sleep(3);

	std::cout << "Printing..." << std::endl;
	
	for(auto e : events){
		dynamicPrintValue(e);
		printf("\n\n\n\n\n");
	}

	/*
	int tot_lines = 0;
	
	for(auto e : events){
		if(e.which() != cereal::Event::CAR_STATE)
			continue;
		tot_lines++;

		//DynamicValue::Reader value = e;
		//auto cs = e.getCarState();
		//std::cout << e.getLogMonoTime() << ": " << cs.getVEgo() << std::endl;
	}

	bool use_provided_configuration = false;

	QProgressDialog progress_dialog;
	progress_dialog.setLabelText("Loading... please wait");
	progress_dialog.setWindowModality(Qt::ApplicationModal);
	progress_dialog.setRange(0, tot_lines - 1);
	progress_dialog.setAutoClose(true);
	progress_dialog.setAutoReset(true);
	progress_dialog.show();

	int linecount = 0;
	bool interrupted = false;
	std::vector<PlotData*> plots_vector;

	std::deque<std::string> valid_field_names;
	valid_field_names.push_back("vEgo");
	valid_field_names.push_back("aEgo");
	valid_field_names.push_back("steeringAngle");
	valid_field_names.push_back("steeringTorque");

	for(auto name : valid_field_names){
		auto it = plot_data.addNumeric(name);
		plots_vector.push_back(&(it->second));
	}

	for(auto e : events){
		if(e.which() != cereal::Event::CAR_STATE)
			continue;

		auto cs = e.getCarState();

		double t = linecount;

		double y = cs.getVEgo();
		PlotData::Point point(t, y);
		plots_vector[0]->pushBack(point);

		y = cs.getAEgo();
		point = PlotData::Point(t, y);
		plots_vector[1]->pushBack(point);

		y = cs.getSteeringAngle();
		point = PlotData::Point(t, y);
		plots_vector[2]->pushBack(point);

		y = cs.getSteeringTorque();
		point = PlotData::Point(t, y);
		plots_vector[3]->pushBack(point);

		if (linecount++ % 100 == 0)
		{
		  progress_dialog.setValue(linecount);
		  QApplication::processEvents();
		  if (progress_dialog.wasCanceled())
		  {
		    interrupted = true;
	            break;
		  }
		}
	}

	if (interrupted)
	{
	  progress_dialog.cancel();
	  plot_data.numeric.clear();
	}
	*/

	return true;
}

bool DataLoadrlog::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const{
	return false;
}

bool DataLoadrlog::xmlLoadState(const QDomElement& parent_element){
	return false;
}

