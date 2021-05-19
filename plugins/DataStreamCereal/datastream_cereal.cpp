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
  ui->lineEditPort->setValidator( new QIntValidator() );
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
  _running(false)
//  sm({"deviceState"})
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

  for( const auto& it: *availableParsers())
  {
    dialog->ui->comboBoxProtocol->addItem( it.first );

    if(auto widget = it.second->optionsWidget() )
    {
      widget->setVisible(false);
      dialog->ui->layoutOptions->addWidget( widget );
    }
  }

  _running = true;
  qDebug() << "started!";

  // load previous values
  QSettings settings;  // todo: could be useful
  QString address = settings.value("ZMQ_Subscriber::address", "localhost").toString();  // todo: need address
  QString protocol = settings.value("ZMQ_Subscriber::protocol", "JSON").toString();  // todo: change this to toggle between zmq and msgq
  int port = settings.value("ZMQ_Subscriber::port", 9872).toInt();


//  dialog->ui->lineEditAddress->setText( address );
//  dialog->ui->lineEditPort->setText( QString::number(port) );
//
  std::shared_ptr<PJ::MessageParserCreator> parser_creator;
//
  connect(dialog->ui->comboBoxProtocol, qOverload<int>( &QComboBox::currentIndexChanged), this,
          [&](int index)
  {
    if( parser_creator ){
      QWidget*  prev_widget = parser_creator->optionsWidget();
      prev_widget->setVisible(false);
    }
    QString protocol = dialog->ui->comboBoxProtocol->itemText(index);
    parser_creator = availableParsers()->at( protocol );

    if(auto widget = parser_creator->optionsWidget() ){
      widget->setVisible(true);
    }
  });
//
//  dialog->ui->comboBoxProtocol->setCurrentText(protocol);
//
//  int res = dialog->exec();
//  if( res == QDialog::Rejected )
//  {
//    _running = false;
//    return false;
//  }

//  address = dialog->ui->lineEditAddress->text();
//  port = dialog->ui->lineEditPort->text().toUShort(&ok);
//  protocol = dialog->ui->comboBoxProtocol->currentText();
//
//  _parser = parser_creator->createInstance({}, dataMap());
//
//  // save back to service
////  settings.setValue("ZMQ_Subscriber::address", address);  // todo: other settings code
////  settings.setValue("ZMQ_Subscriber::protocol", protocol);
////  settings.setValue("ZMQ_Subscriber::port", port);
//
//  _socket_address =
//      (dialog->ui->comboBox->currentText()+
//      address+ ":" + QString::number(port)).toStdString();
//
////  _zmq_socket.connect( _socket_address.c_str() );
////  // subscribe to everything
////  _zmq_socket.set(zmq::sockopt::subscribe, "");
////  _zmq_socket.set(zmq::sockopt::rcvtimeo, 100);
//
//  qDebug() << "ZMQ listening on address" << QString::fromStdString( _socket_address );
//  _running = true;
//
  _receive_thread = std::thread(&DataStreamCereal::receiveLoop, this);
//
//  dialog->deleteLater();
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
  Context * c = Context::create();  // todo: move this to creation of class and make sure to delete when exiting
  Poller * poller = Poller::create();

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
  std::map<std::string, SubSocket *> services;
//  std::map<std::string, SubSocket *> sockets;  # todo: messages (subsocket, submessage)

  const char *address;


  for (auto field : event_struct_schema.getUnionFields()) {
    std::string name = field.getProto().getName();
    service_list.push_back(name);

    SubSocket *sock = SubSocket::create(c, name, address ? address : "127.0.0.1");  // no conflating
    assert(sock != NULL);
    poller->registerSocket(sock);

    services[name] = sock;
    qDebug() << "Added service and socket:" << name.c_str();
  }

  void *allocated_msg_reader = nullptr;
  AlignedBuffer aligned_buf;
  allocated_msg_reader = malloc(sizeof(capnp::FlatArrayMessageReader));

  qDebug() << "entering receive loop...";
  while( _running )
  {
    auto start = std::chrono::high_resolution_clock::now();

//    try {
      std::lock_guard<std::mutex> lock(mutex());
      for (auto sock : poller->poll(0)) {  // 10 for 100 hz? probably 0
        // drain socket
        while (_running) {
          Message *msg = sock->receive(true);
          if (!msg){
            break;
          }

//          capnp::FlatArrayMessageReader *msg_reader = new (malloc(sizeof(capnp::FlatArrayMessageReader))) capnp::FlatArrayMessageReader({});
          capnp::FlatArrayMessageReader *msg_reader = new (allocated_msg_reader) capnp::FlatArrayMessageReader(aligned_buf.align(msg));
          cereal::Event::Reader event = msg_reader->getRoot<cereal::Event>();
//          event.getLogMonoTime();
          double time_stamp = (double)event.getLogMonoTime() / 1e9;
          parser.parseMessageImpl("", event, time_stamp, show_deprecated);
//          for (auto field : event.getUnionFields()) {
//            std::string name = field.getProto().getName();
//            qDebug() << name.c_str();
//          }
//          qDebug() << "here!";
//          capnp::FlatArrayMessageReader cmsg = capnp::FlatArrayMessageReader(msg);
//          capnp::FlatArrayMessageReader *tmsg = new capnp::FlatArrayMessageReader(kj::ArrayPtr(msg.begin(), msg.getEnd()));
//          capnp::DynamicStruct::Reader *tmsg = (capnp::DynamicStruct::Reader*)msg->getData();
//          capnp::DynamicStruct::Reader event = tmsg->getRoot<capnp::DynamicStruct>(event_struct_schema);
//          double time_stamp = (double)event->getLogMonoTime() / 1e9;

        }
      }

//      for (const std::string service_name : service_list)
//      {
////        qDebug() << "checking if" << service_name << "updated";
//        if (sm.updated(service_name))
//        {
//          auto event = sm[service_name];
//          double time_stamp = (double)event.getLogMonoTime() / 1e9;  // fixme: it's an int when printed?
//          qDebug() << time_stamp;
//
//          parser.parseMessageImpl("", event, time_stamp, show_deprecated);
//        }
//      }

//    }
//    catch (std::exception& err)  // todo: do some disconnect?
//    {
//      QMessageBox::warning(nullptr,
//                           tr("ZMQ Subscriber"),
//                           tr("Problem parsing the message. ZMQ Subscriber will be stopped.\n%1").arg(err.what()),
//                           QMessageBox::Ok);
//
//      _running = false;
//      // notify the GUI
//      emit closed();
//      return;
//    }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  qDebug() << "Loop time:" << duration << "ms";

  }
}


