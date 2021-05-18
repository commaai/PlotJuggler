#include "datastream_cereal.h"
#include "ui_datastream_cereal.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDialog>
#include <QIntValidator>
#include <chrono>

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
  _running(false),
  sm({"deviceState"})
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
//  QSettings settings;  // todo: could be useful
//  QString address = settings.value("ZMQ_Subscriber::address", "localhost").toString();
//  QString protocol = settings.value("ZMQ_Subscriber::protocol", "JSON").toString();
//  int port = settings.value("ZMQ_Subscriber::port", 9872).toInt();


//  dialog->ui->lineEditAddress->setText( address );
//  dialog->ui->lineEditPort->setText( QString::number(port) );
//
//  std::shared_ptr<PJ::MessageParserCreator> parser_creator;
//
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
//
//  dialog->ui->comboBoxProtocol->setCurrentText(protocol);
//
//  int res = dialog->exec();
//  if( res == QDialog::Rejected )
//  {
//    _running = false;
//    return false;
//  }
//
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
  qDebug() << "entering receive loop...";
  while( _running )
  {
    sm.update(0);
    qDebug() << "battery temp:" << sm["deviceState"].getDeviceState().getBatteryTempC();
//    zmq::message_t recv_msg;
//    zmq::recv_result_t result = _zmq_socket.recv(recv_msg);

//    if( recv_msg.size() > 0 )
//    {
//      using namespace std::chrono;
//      auto ts = high_resolution_clock::now().time_since_epoch();
//      double timestamp = 1e-6* double( duration_cast<microseconds>(ts).count() );
//
//      PJ::MessageRef msg ( reinterpret_cast<uint8_t*>(recv_msg.data()), recv_msg.size() );
//
//      try {
//        std::lock_guard<std::mutex> lock(mutex());
//        _parser->parseMessage(msg, timestamp);
//      } catch (std::exception& err)
//      {
//        QMessageBox::warning(nullptr,
//                             tr("ZMQ Subscriber"),
//                             tr("Problem parsing the message. ZMQ Subscriber will be stopped.\n%1").arg(err.what()),
//                             QMessageBox::Ok);
//
//        _zmq_socket.disconnect( _socket_address.c_str() );
//        _running = false;
//        // notify the GUI
//        emit closed();
//        return;
//      }
//    }
  }
}


