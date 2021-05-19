#pragma once
#include <QDialog>
#include <QDir>
#include <unistd.h>
#include <pwd.h>

#include <QtPlugin>
#include <thread>
#include "PlotJuggler/datastreamer_base.h"
#include "PlotJuggler/messageparser_base.h"
#include "ui_datastream_cereal.h"
#include "../plugins/DataLoadRlog/rlog_parser.hpp"

#include "cereal/messaging/messaging.h"
#include <PlotJuggler/dataloader_base.h>
#include <PlotJuggler/datastreamer_base.h>
#include <capnp/serialize-packed.h>
#include <capnp/schema-parser.h>
//#include <cereal_parser.hpp>
#include "services.h"


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
//  SubMaster sm;  // todo: replace with sm and pm (or just sm if we make another plugin for publishing)
//  zmq::socket_t _zmq_socket;
//  PJ::MessageParserPtr _parser;
//  std::string zmq_address;
//  std::string _socket_address;  # we need multiple socket addresses
  std::thread _receive_thread;

  void receiveLoop();
};
