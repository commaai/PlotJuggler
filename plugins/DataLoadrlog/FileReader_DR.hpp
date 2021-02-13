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

#include "/home/batman/openpilot/tools/clib/channel.hpp"
#include <thread>

using namespace PJ;

class FileReader : public QObject {

/* 
  Edits:
  - commented out URL functionality
  - adding bz2 file loading
  
*/

  Q_OBJECT

public:
  FileReader(const QString& file_);
  ~FileReader();
  virtual void readyRead();
  virtual void done() {};

public slots:
  void process();

protected:
  QFile* loaded_file;

private:
  QString file;
};

typedef QMultiMap<uint64_t, capnp::DynamicStruct::Reader> Events;

class LogReader : public FileReader {
Q_OBJECT
public:
  LogReader(const QString& file, Events* events_);
  ~LogReader();

  void readyRead();
  void done() { is_done = true; };
  bool is_done = false;

  void mergeEvents(int dled);
  channel<int> cdled;

private:
  bz_stream bStream;

  // backing store
  QByteArray raw;

  std::thread *parser;
  int event_offset;
  Events* events;

};

