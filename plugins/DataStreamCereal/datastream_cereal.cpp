#include "datastream_cereal.h"
#include "ui_datastream_cereal.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDialog>
#include <QIntValidator>
#include <chrono>
#include <assert.h>


int new_port(int port) {
  int RESERVED_PORT = 8022;
  int STARTING_PORT = 8001;
  port += STARTING_PORT;
  return port >= RESERVED_PORT ? port + 1 : port;
}

//struct SubMessage {  // todo: old
//  std::string name;
//  SubSocket *socket = nullptr;
//  int freq = 0;
//  bool updated = false, alive = false, valid = true, ignore_alive;
//  uint64_t rcv_time = 0, rcv_frame = 0;
//  void *allocated_msg_reader = nullptr;
//  capnp::FlatArrayMessageReader *msg_reader = nullptr;
//  AlignedBuffer aligned_buf;
//  cereal::Event::Reader event;
//};


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
  poller(Poller::create()),
  c(Context::create())  // todo: make sure to delete when exiting
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
  QSettings settings;  // todo: could be useful
  QString address = settings.value("ZMQ_Subscriber::address", "192.168.43.1").toString();
  QString protocol = settings.value("ZMQ_Subscriber::protocol", "JSON").toString();  // todo: change this to toggle between zmq and msgq
//  int port = settings.value("ZMQ_Subscriber::port", 9872).toInt();


  dialog->ui->lineEditAddress->setText( address );
//  dialog->ui->lineEditPort->setText( QString::number(port) );

  std::shared_ptr<PJ::MessageParserCreator> parser_creator;

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

  int res = dialog->exec();
  if( res == QDialog::Rejected )
  {
    _running = false;
    return false;
  }

  address = dialog->ui->lineEditAddress->text();
//  port = dialog->ui->lineEditPort->text().toUShort(&ok);
//  protocol = dialog->ui->comboBoxProtocol->currentText();

//  _parser = parser_creator->createInstance({}, dataMap());  // TODO: replace with rlog parser

  // save for next time
  settings.setValue("ZMQ_Subscriber::address", address);  // todo: other settings code
//  settings.setValue("ZMQ_Subscriber::protocol", protocol);
//  settings.setValue("ZMQ_Subscriber::port", port);

//  zmq_address = address.toStdString();

//  _socket_address =
//      (dialog->ui->comboBox->currentText()+
//      address+ ":" + QString::number(port)).toStdString();

//  _zmq_socket.connect( _socket_address.c_str() );
//  // subscribe to everything
//  _zmq_socket.set(zmq::sockopt::subscribe, "");
//  _zmq_socket.set(zmq::sockopt::rcvtimeo, 100);

//  qDebug() << "ZMQ listening on address" << QString::fromStdString( _socket_address );
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
  QString schema_path(std::getenv("BASEDIR"));
  bool show_deprecated = std::getenv("SHOW_DEPRECATED");

  if (schema_path.isNull())
  {
    schema_path = QDir(getpwuid(getuid())->pw_dir).filePath("openpilot/openpilot"); // fallback to $HOME/openpilot (fixme: temp dir)
  }
  schema_path = QDir(schema_path).filePath("cereal/log.capnp");
  schema_path.remove(0, 1);

  // Parse the schema
  auto fs = kj::newDiskFilesystem();

  capnp::SchemaParser schema_parser;
  capnp::ParsedSchema schema = schema_parser.parseFromDirectory(fs->getRoot(), kj::Path::parse(schema_path.toStdString()), nullptr);
  capnp::StructSchema event_struct_schema = schema.getNested("Event").asStruct();

  PlotDataMapRef& plot_data = dataMap();
  RlogMessageParser parser("", plot_data);


  std::vector<std::string> service_list;
//  std::map<SubSocket *, SubMessage *> messages;
//  std::map<std::string, SubMessage *> services;
  std::vector<SubSocket *> _services;

//  std::map<std::string, SubSocket *> sockets;  # todo: messages (subsocket, submessage)

  // detect if zmq or not here, msgq address should be 127.0.0.1
  std::string address = "192.168.43.1";
//  std::string address = zmq_address;
  bool use_zmq = true;
  int idx = 0;
//  for (auto field : event_struct_schema.getUnionFields()) {
  for (const auto &it : services) {
    std::string name = std::string(it.name);
    service_list.push_back(name);

    // todo: support zmq and custom addresses
    int port = new_port(idx);
    qDebug() << "port:" << port;


    SubSocket *socket;

//    std::string endpoint = std::string("tcp://192.168.43.1:") + std::to_string(port);
//    qDebug() << "endpoint:" << endpoint.c_str();
    socket = SubSocket::create(c, name, address, true, true);

    assert(socket != 0);
    poller->registerSocket(socket);
//    SubMessage *m = new SubMessage{
//      .name = name,
//      .socket = socket,
//      .freq = 0,
//      .ignore_alive = true,
//      .allocated_msg_reader = malloc(sizeof(capnp::FlatArrayMessageReader))};
//    m->msg_reader = new (m->allocated_msg_reader) capnp::FlatArrayMessageReader({});
//    messages[socket] = m;
//    services[name] = m;
    _services.push_back(socket);
    qDebug() << "Added service and socket:" << name.c_str() << "with address:";
    idx++;
  }

  void *allocated_msg_reader = malloc(sizeof(capnp::FlatArrayMessageReader));
  AlignedBuffer aligned_buf;
  capnp::FlatArrayMessageReader *msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader({});

  qDebug() << "entering receive loop...";
  while( _running )
  {
//    qDebug() << "loop";
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


