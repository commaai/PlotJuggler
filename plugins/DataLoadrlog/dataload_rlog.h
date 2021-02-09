#pragma once

#include <QObject>
#include <QtPlugin>
#include "PlotJuggler/dataloader_base.h"
#include <QThread>
#include <fstream>
#include <QDebug>
#include <QObject>
#include <QProgressDialog>
#include <QtNetwork>

#include <unistd.h>
#include <bzlib.h>

#include <capnp/schema.h>
#include <capnp/dynamic.h>
#include <capnp/serialize-packed.h>

#include <capnp/schema-parser.h>

#include "../../../../../cereal/gen/cpp/log.capnp.h"
#include "../../../../clib/channel.hpp"
#include <thread>

#include "FileReader_DR.hpp"

using namespace PJ;

class DataLoadrlog : public DataLoader {

	Q_OBJECT
	Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataLoader")
	Q_INTERFACES(PJ::DataLoader)

public:
	DataLoadrlog();
	virtual ~DataLoadrlog();

	virtual bool readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& plot_data);

	virtual const std::vector<const char*>& compatibleFileExtensions() const override;

	virtual const char* name() const override {
		return "DataLoad rlog";
	}

	virtual bool xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const override;

	virtual bool xmlLoadState(const QDomElement& parent_element) override;


protected:


private:
	std::vector<const char*> _extensions;
	std::string _default_time_axis;

};




