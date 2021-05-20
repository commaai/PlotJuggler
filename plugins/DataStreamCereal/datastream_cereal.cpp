#include "datastream_cereal.h"
#include "ui_datastream_cereal.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDialog>
#include <QIntValidator>
#include <assert.h>


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
  show_deprecated(std::getenv("SHOW_DEPRECATED"))
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

  StreamCerealDialog* dialog = new StreamCerealDialog();
  bool use_zmq = std::getenv("ZMQ");
  if (use_zmq)
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
    address = dialog->ui->lineEditAddress->text();
    // save for next time
    settings.setValue("Cereal_Subscriber::address", address);
    qDebug() << "Using address:" << address.toStdString().c_str();
  }
  else
  {
    qDebug() << "Using MSGQ backend!";
    address = "127.0.0.1";
  }

  c = Context::create();
  poller = Poller::create();
  for (const auto &it : services)
  {
    std::string name = std::string(it.name);

    SubSocket *socket;
    socket = SubSocket::create(c, name, address.toStdString(), false, true);  // don't conflate
    assert(socket != 0);
    socket->setTimeout(0);

    poller->registerSocket(socket);
    _services.push_back(socket);
  }

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

    for (auto sock : _services)
    {
      delete sock;
    }
    _services.clear();

    delete c;
    delete poller;

    _running = false;
  }
}

void DataStreamCereal::receiveLoop()
{
  PlotDataMapRef& plot_data = dataMap();
  RlogMessageParser parser("", plot_data);

  void *allocated_msg_reader = malloc(sizeof(capnp::FlatArrayMessageReader));
  AlignedBuffer aligned_buf;
  capnp::FlatArrayMessageReader *msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader({});

  qDebug() << "Entering receive thread...";
  while (_running)
  {
    while (_running)
    {
      auto sockets = poller->poll(0);
      if (sockets.size() == 0)
        break;

      for (auto sock : sockets)
      {
        Message * msg = sock->receive(true);
        if (msg == nullptr)
          break;

        msg_reader->~FlatArrayMessageReader();
        msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader(aligned_buf.align(msg));
        delete msg;
        cereal::Event::Reader event = msg_reader->getRoot<cereal::Event>();
        double time_stamp = (double)event.getLogMonoTime() / 1e9;
        try
        {
          std::lock_guard<std::mutex> lock(mutex());
          parser.parseMessageImpl("", event, time_stamp, show_deprecated);
        }
        catch (std::exception& err)
        {
          QMessageBox::warning(nullptr,
                               tr("Cereal Subscriber"),
                               tr("Problem parsing the message. Cereal Subscriber will be stopped.\n%1").arg(err.what()),
                               QMessageBox::Ok);

          _running = false;
          // notify the GUI
          emit closed();
          return;
        }
      }
    }
  }
}
