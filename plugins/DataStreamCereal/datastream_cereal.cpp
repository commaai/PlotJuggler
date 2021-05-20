#include "datastream_cereal.h"
#include "ui_datastream_cereal.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDialog>
#include <QIntValidator>
#include <chrono>
#include <assert.h>


StreamCerealDialog::StreamCerealDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::DataStreamCereal)
{
  ui->setupUi(this);
//  ui->lineEditPort->setValidator( new QIntValidator() );
}

StreamCerealDialog::~StreamCerealDialog()
{
  while( ui->layoutOptions->count() > 0)
  {
    auto item = ui->layoutOptions->takeAt(0);
    item->widget()->setParent(nullptr);
  }
  delete ui;
}

DataStreamCereal::DataStreamCereal():
  _running(false),
  c(Context::create()),  // todo: make sure to delete when exiting
  poller(Poller::create()),
  use_zmq(std::getenv("ZMQ")),
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

  if( !availableParsers() )
  {
    QMessageBox::warning(nullptr,tr("UDP Server"), tr("No available MessageParsers"),  QMessageBox::Ok);
    _running = false;
    return false;
  }

  bool ok = false;

  StreamCerealDialog* dialog = new StreamCerealDialog();

//  _running = true;
//  qDebug() << "started!";

  // load previous values

//  dialog->ui->lineEditPort->setText( QString::number(port) );

//  std::shared_ptr<PJ::MessageParserCreator> parser_creator;

//  connect(dialog->ui->comboBoxProtocol, qOverload<int>( &QComboBox::currentIndexChanged), this,
//          [&](int index)
//  {
//    if( parser_creator ){
//      QWidget*  prev_widget = parser_creator->optionsWidget();
//      prev_widget->setVisible(false);
//    }
//    QString protocol = dialog->ui->comboBoxProtocol->itemText(index);
//    parser_creator = availableParsers()->at( protocol );
//
//    if(auto widget = parser_creator->optionsWidget() ){
//      widget->setVisible(true);
//    }
//  });

//  dialog->ui->comboBoxProtocol->setCurrentText(protocol);

  if (use_zmq) {
    qDebug() << "Using ZMQ backend!";

    QSettings settings;
    address = settings.value("ZMQ_Subscriber::address", "192.168.43.1").toString();

    dialog->ui->lineEditAddress->setText( address );

    int res = dialog->exec();
    if( res == QDialog::Rejected )
    {
      _running = false;
      return false;
    }
    address = dialog->ui->lineEditAddress->text();
    // save for next time
    settings.setValue("ZMQ_Subscriber::address", address);  // todo: other settings code
    qDebug() << "Using address in start1:" << address.toStdString().c_str();
  } else {
    qDebug() << "Using MSGQ backend!";
    address = "127.0.0.1";
  }

//  std::string use_address = address.toStdString();
//  qDebug() << "Using address in start:" << use_address.c_str();
  qDebug() << "Using address in start2:" << address.toStdString().c_str();


//  std::vector<SubSocket *> _services;  // TODO: we don't need to keep track of the sockets, just register with poller. remove?

  std::string use_address = address.toStdString();
  qDebug() << "Using address:" << use_address.c_str();

  for (const auto &it : services) {
    std::string name = std::string(it.name);

    SubSocket *socket;
    socket = SubSocket::create(c, name, use_address, true, true);
    assert(socket != 0);
    poller->registerSocket(socket);
//    _services.push_back(socket);
  }


  _running = true;

  _receive_thread = std::thread(&DataStreamCereal::receiveLoop, this);

  dialog->deleteLater();
  return _running;
}

void DataStreamCereal::shutdown()
{
  if( _running )
  {
    _running = false;
    if( _receive_thread.joinable() )
    {
      _receive_thread.join();
    }
    // todo: do any SubMaster shutdown here
//    _zmq_socket.disconnect( _socket_address.c_str() );
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

  qDebug() << "entering receive loop...";
  while( _running )
  {
    auto start = std::chrono::high_resolution_clock::now();
    try {
      while ( _running ) {
        auto sockets = poller->poll(0);
        if (sockets.size() == 0)
          break;

        for (auto sock : sockets) {
          Message * msg = sock->receive(true);
          if (msg == nullptr)
            break;

          msg_reader->~FlatArrayMessageReader();
          msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader(aligned_buf.align(msg));
          delete msg;
          cereal::Event::Reader event = msg_reader->getRoot<cereal::Event>();
          double time_stamp = (double)event.getLogMonoTime() / 1e9;
          std::lock_guard<std::mutex> lock(mutex());
          parser.parseMessageImpl("", event, time_stamp, show_deprecated);
        }

//        for (auto sock : sockets) {
//          Message * msg = sock->receive();
//          if (msg == NULL) continue;
//          // parse msg
//          delete msg;
//        }
      }
//      for (auto sock : poller->poll(0)) {
//        while (_running) {
//          Message *msg = sock->receive(true);
//          if (!msg){
//            break;
//          }
//
//          capnp::FlatArrayMessageReader *msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader(aligned_buf.align(msg));
//          cereal::Event::Reader event = msg_reader->getRoot<cereal::Event>();
//          double time_stamp = (double)event.getLogMonoTime() / 1e9;
////          std::lock_guard<std::mutex> lock(mutex());
//          parser.parseMessageImpl("", event, time_stamp, show_deprecated);
//        }
//      }
    }
    catch (std::exception& err)  // todo: do some disconnect?
    {
      QMessageBox::warning(nullptr,
                           tr("ZMQ Subscriber"),
                           tr("Problem parsing the message. ZMQ Subscriber will be stopped.\n%1").arg(err.what()),
                           QMessageBox::Ok);

      _running = false;
      // notify the GUI
      emit closed();
      return;
    }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  if (duration > 2000) {
    if (duration > 5000) qDebug() << "Warning!";
    qDebug() << "Loop time:" << (float)duration / 1000.0 << "ms";
  }
  }
}


