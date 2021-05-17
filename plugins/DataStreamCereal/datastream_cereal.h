#pragma once
#include <QDialog>

#include <QtPlugin>
#include <thread>
#include "PlotJuggler/datastreamer_base.h"
#include "PlotJuggler/messageparser_base.h"
#include "ui_datastream_cereal.h"
//#include "cereal/gen/cpp/car.capnp.h"
#include "cereal/messaging/messaging.h"

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
  zmq::context_t _zmq_context;  // todo: replace with sm and pm (or just sm if we make another plugin for publishing)
  zmq::socket_t _zmq_socket;
  PJ::MessageParserPtr _parser;
  std::string _socket_address;
  std::thread _receive_thread;

  void receiveLoop();
};
