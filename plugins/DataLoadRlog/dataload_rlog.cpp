#include <dataload_rlog.hpp>

QByteArray* load_bytes(const char* fn)
{
  int bzError;
  FILE* f = fopen(fn, "rb");
  QByteArray* raw = new QByteArray;
  // 64MB buffer
  raw->resize(1024*1024*64);
  
  BZFILE *bytes = BZ2_bzReadOpen(&bzError, f, 0, 0, NULL, 0);
  if(bzError != BZ_OK && bzError != BZ_STREAM_END) qWarning() << "bz2 decompress failed";
  int dled = BZ2_bzRead(&bzError, bytes, raw->data(), raw->size());
  raw->resize(dled);

  if(bzError != BZ_STREAM_END) qWarning() << "buffer size too small for log";
  BZ2_bzReadClose(&bzError, bytes);

  return raw;
}

DataLoadRlog::DataLoadRlog()
{
  _extensions.push_back("bz2"); 
}

DataLoadRlog::~DataLoadRlog()
{
}

const std::vector<const char*>& DataLoadRlog::compatibleFileExtensions() const
{
  return _extensions;
}

bool DataLoadRlog::readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& plot_data)
{

  QProgressDialog progress_dialog;
  progress_dialog.setLabelText("Decompressing log...");
  progress_dialog.setWindowModality(Qt::ApplicationModal);
  progress_dialog.show();

  auto fn = fileload_info->filename;
  qDebug() << "Loading: " << fn;

  // Load file:
  int event_offset = 0;
  QByteArray* raw = load_bytes(fn.toStdString().c_str());

  kj::ArrayPtr<const capnp::word> amsg = kj::arrayPtr((const capnp::word*)(raw->data() + event_offset), (raw->size()-event_offset)/sizeof(capnp::word));

  int max_amsg_size = amsg.size();

  progress_dialog.setLabelText("Parsing log...");
  progress_dialog.setRange(0, max_amsg_size);
  progress_dialog.show();

  QString openpilot_dir(std::getenv("BASEDIR"));
  if(openpilot_dir.isNull())
  {
    openpilot_dir = QDir(getpwuid(getuid())->pw_dir).filePath("openpilot");
  }
  openpilot_dir = QDir(openpilot_dir).filePath("cereal/log.capnp");
  openpilot_dir.remove(0, 1);

  //Parse the schema:
  auto fs = kj::newDiskFilesystem();
  capnp::SchemaParser schema_parser;
  capnp::ParsedSchema schema = schema_parser.parseFromDirectory(fs->getRoot(), kj::Path::parse(openpilot_dir.toStdString()), nullptr);
  capnp::ParsedSchema event = schema.getNested("Event");
  capnp::StructSchema event_struct = event.asStruct();

  RlogMessageParser parser("", plot_data);

  while(amsg.size() > 0)
  {
    try
    {
      capnp::FlatArrayMessageReader cmsg = capnp::FlatArrayMessageReader(amsg);

      capnp::FlatArrayMessageReader *tmsg = new capnp::FlatArrayMessageReader(kj::arrayPtr(amsg.begin(), cmsg.getEnd()));
      amsg = kj::arrayPtr(cmsg.getEnd(), amsg.end());

      capnp::DynamicStruct::Reader event_example = tmsg->getRoot<capnp::DynamicStruct>(event_struct);

      parser.parseMessageImpl("", event_example, (double)event_example.get("logMonoTime").as<uint64_t>() / 1e9);

      // increment
      event_offset = (char*)cmsg.getEnd() - raw->data();
    }
    catch (const kj::Exception& e)
    {
      break;
    }

    progress_dialog.setValue(max_amsg_size - amsg.size());
    QApplication::processEvents();
    if(progress_dialog.wasCanceled())
    {
      return false;
    }
  }

  return true;
}

bool DataLoadRlog::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  return false;
}

bool DataLoadRlog::xmlLoadState(const QDomElement& parent_element)
{
  return false;
}
