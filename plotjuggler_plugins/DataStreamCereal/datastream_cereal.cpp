#include <assert.h>

#include <QDebug>
#include <QDialog>
#include <QIntValidator>
#include <QMessageBox>
#include <QSettings>

#include "datastream_cereal.h"
#include "ui_datastream_cereal.h"


StreamCerealDialog::StreamCerealDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::DataStreamCereal)
{
  ui->setupUi(this);
}

StreamCerealDialog::~StreamCerealDialog()
{
  while (ui->layoutOptions->count() > 0)
  {
    auto item = ui->layoutOptions->takeAt(0);
    item->widget()->setParent(nullptr);
  }
  delete ui;
}

DataStreamCereal::DataStreamCereal():
  _running(false),
  parser(RlogMessageParser("", dataMap(), true))
{
}

DataStreamCereal::~DataStreamCereal()
{
  shutdown();
}

bool DataStreamCereal::start(QStringList*)
{
  if (_running)
  {
    return _running;
  }

  QString address;
  StreamCerealDialog* dialog = new StreamCerealDialog();
  if (std::getenv("ZMQ"))
  {
    qDebug() << "Using ZMQ backend!";

    QSettings settings;
    address = settings.value("Cereal_Subscriber::address", "192.168.43.1").toString();
    dialog->ui->lineEditAddress->setText(address);

    int res = dialog->exec();
    if (res == QDialog::Rejected)
    {
      _running = false;
      return false;
    }
    // save for next time
    address = dialog->ui->lineEditAddress->text();
    settings.setValue("Cereal_Subscriber::address", address);
  }
  else
  {
    qDebug() << "Using MSGQ backend!";
    address = "127.0.0.1";
  }

  c = Context::create();
  poller = Poller::create();
  for (const auto &serv : services)
  {
    SubSocket *socket;
    socket = SubSocket::create(c, std::string(serv.name), address.toStdString(), false, true);  // don't conflate
    assert(socket != 0);

    poller->registerSocket(socket);
    sockets.push_back(socket);
  }

  parser.selectDBCDialog();  // can't show dialog in a thread so show it now
  _running = true;
  _receive_thread = std::thread(&DataStreamCereal::receiveLoop, this);

  dialog->deleteLater();
  return _running;
}

void DataStreamCereal::shutdown()
{
  if (_running)
  {
    _running = false;
    if (_receive_thread.joinable())
    {
      _receive_thread.join();
    }

    for (auto sock : sockets)
    {
      delete sock;
    }
    sockets.clear();  // clear deleted sockets

    delete c;
    delete poller;

    _running = false;
  }
}

void DataStreamCereal::receiveLoop()
{
  AlignedBuffer aligned_buf;

  while (_running)
  {
    for (auto sock : poller->poll(100))
    {
      while (_running)  // drain socket
      {
        Message *msg = sock->receive(true);
        if (msg == nullptr)
          break;

        capnp::FlatArrayMessageReader cmsg(aligned_buf.align(msg));
        capnp::DynamicStruct::Reader event = cmsg.getRoot<capnp::DynamicStruct>(parser.getSchema());
        try
        {
          std::lock_guard<std::mutex> lock(mutex());
          parser.parseMessageCereal(event);
          emit this->dataReceived();
        }
        catch (std::exception& err)
        {
          qWarning() << "Cereal Streamer: problem parsing the message, continuing...";
        }
        delete msg;
      }
    }
  }
}
