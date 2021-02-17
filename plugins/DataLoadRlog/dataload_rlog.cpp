#include <dataload_rlog.hpp>

QByteArray* read_file(const char* fn)
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
  fclose(f);

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
  QByteArray* raw = read_file(fn.toStdString().c_str());

  kj::ArrayPtr<const capnp::word> amsg = kj::ArrayPtr((const capnp::word*)raw->data(), raw->size()/sizeof(capnp::word));

  int max_amsg_size = amsg.size();

  progress_dialog.setLabelText("Parsing log...");
  progress_dialog.setRange(0, max_amsg_size);
  progress_dialog.show();

  QString schema_path(std::getenv("BASEDIR"));
  if(schema_path.isNull())
  {
    schema_path = QDir(getpwuid(getuid())->pw_dir).filePath("openpilot"); // fallback to $HOME/openpilot
  }
  schema_path = QDir(schema_path).filePath("cereal/log.capnp");
  schema_path.remove(0, 1);

  // Parse the schema
  auto fs = kj::newDiskFilesystem();

  capnp::SchemaParser schema_parser;
  capnp::ParsedSchema schema = schema_parser.parseFromDirectory(fs->getRoot(), kj::Path::parse(schema_path.toStdString()), nullptr);
  capnp::StructSchema event_struct = schema.getNested("Event").asStruct();

  RlogMessageParser parser("", plot_data);

  while(amsg.size() > 0)
  {
    try
    {
      capnp::FlatArrayMessageReader cmsg = capnp::FlatArrayMessageReader(amsg);
      capnp::FlatArrayMessageReader *tmsg = new capnp::FlatArrayMessageReader(kj::ArrayPtr(amsg.begin(), cmsg.getEnd()));
      amsg = kj::ArrayPtr(cmsg.getEnd(), amsg.end());

      capnp::DynamicStruct::Reader event_example = tmsg->getRoot<capnp::DynamicStruct>(event_struct);

      parser.parseMessageImpl("", event_example, (double)event_example.get("logMonoTime").as<uint64_t>() / 1e9);
    }
    catch (const kj::Exception& e)
    { 
      std::cerr << e.getDescription().cStr() << std::endl;
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
