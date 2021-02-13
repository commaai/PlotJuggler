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

void dynamicPrintValue(capnp::DynamicValue::Reader value);

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
  LogReader(const QString& file, Events* events_, QReadWriteLock* events_lock_, QMap<int, QPair<int, int> > *eidx_);
  ~LogReader();

  void readyRead();
  void done() { is_done = true; };
  bool is_done = false;

private:
  bz_stream bStream;

  // backing store
  QByteArray raw;

  std::thread *parser;
  int event_offset;
  channel<int> cdled;
  Events* events;

  // global                                                                                              
  void mergeEvents(int dled);
  QReadWriteLock* events_lock;
  QMap<int, QPair<int, int> > *eidx;
};

