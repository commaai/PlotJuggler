#pragma once
#include <pwd.h>
#include <unistd.h>

#include <capnp/schema-parser.h>
#include <capnp/serialize-packed.h>

#include <thread>
#include <QDialog>
#include <QDir>
#include <QtPlugin>

#include "cereal/messaging/messaging.h"
#include "cereal/services.h"
#include "PlotJuggler/datastreamer_base.h"
#include "PlotJuggler/dataloader_base.h"
#include "ui_datastream_cereal.h"
#include "DataLoadRlog/rlog_parser.hpp"


class StreamCerealDialog : public QDialog
{
  Q_OBJECT

public:
  explicit StreamCerealDialog(QWidget *parent = nullptr);
  ~StreamCerealDialog();

  Ui::DataStreamCereal *ui;

};

class DataStreamCereal : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamCereal();

  virtual ~DataStreamCereal() override;

  virtual bool start(QStringList*) override;

  virtual void shutdown() override;

  virtual bool isRunning() const override
  {
    return _running;
  }

  virtual const char* name() const override
  {
    return "Cereal Subscriber";
  }

  virtual bool isDebugPlugin() override
  {
    return false;
  }

private:
  bool _running;
  Context *c;
  Poller *poller;
  bool show_deprecated;
  std::vector<SubSocket *> sockets;
  QString address;
  std::thread _receive_thread;

  void receiveLoop();
};
