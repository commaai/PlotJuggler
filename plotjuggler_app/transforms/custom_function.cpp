#include "custom_function.h"

#include <limits>
#include <QFile>
#include <QMessageBox>
#include <QElapsedTimer>
#include "lua_custom_function.h"

CustomFunction::CustomFunction(SnippetData snippet)
{
  setSnippet(snippet);
}

void CustomFunction::setSnippet(const SnippetData& snippet)
{
  _snippet = snippet;
  _linked_plot_name = snippet.linked_source.toStdString();
  _plot_name = snippet.alias_name.toStdString();

  _used_channels.clear();
  for (QString source : snippet.additional_sources)
  {
    _used_channels.push_back(source.toStdString());
  }
}

void CustomFunction::reset()
{
  // This cause a crash during streaming for reasons that are not 100% clear.
  // initEngine();
}

void CustomFunction::calculateAndAdd(PlotDataMapRef& src_data)
{
  bool newly_added = false;

  auto dst_data_it = src_data.numeric.find(_plot_name);
  if (dst_data_it == src_data.numeric.end())
  {
    dst_data_it = src_data.addNumeric(_plot_name);
    newly_added = true;
  }

  PlotData& dst_data = dst_data_it->second;
  std::vector<PlotData*> dst_vector = { &dst_data };
  dst_data.clear();

  setData(&src_data, {}, dst_vector);

  try
  {
    calculate();
  }
  catch (...)
  {
    if (newly_added)
    {
      plotData()->numeric.erase(dst_data_it);
    }
    std::rethrow_exception(std::current_exception());
  }
}

const SnippetData& CustomFunction::snippet() const
{
  return _snippet;
}

void CustomFunction::calculate()
{
  auto dst_data = _dst_vector.front();

  auto data_it = plotData()->numeric.find(_linked_plot_name);
  if (data_it == plotData()->numeric.end())
  {
    // failed! keep it empty
    return;
  }
  _src_vector.clear();
  _src_vector.push_back(&data_it->second);

  for (const auto& channel : _used_channels)
  {
    auto it = plotData()->numeric.find(channel);
    if (it == plotData()->numeric.end())
    {
      throw std::runtime_error("Invalid channel name");
    }
    const PlotData* chan_data = &(it->second);
    _src_vector.push_back(chan_data);
  }

  const PlotData* main_data_source = _src_vector.front();

  // clean up old data
  dst_data->setMaximumRangeX(main_data_source->maximumRangeX());

  double last_updated_stamp = std::numeric_limits<double>::lowest();
  if (dst_data->size() != 0)
  {
    last_updated_stamp = dst_data->back().x;
  }

  std::vector<PlotData::Point> points;
  for (size_t i = 0; i < main_data_source->size(); ++i)
  {
    if (main_data_source->at(i).x > last_updated_stamp)
    {
      points.clear();
      calculatePoints(_src_vector, i, points);

      for (PlotData::Point const& point : points)
      {
        dst_data->pushBack(point);
      }
    }
  }
}

bool CustomFunction::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  parent_element.appendChild(ExportSnippetToXML(_snippet, doc));
  return true;
}

bool CustomFunction::xmlLoadState(const QDomElement& parent_element)
{
  setSnippet(GetSnippetFromXML(parent_element));
  return true;
}

SnippetsMap GetSnippetsFromXML(const QString& xml_text)
{
  if (xml_text.isEmpty())
  {
    return {};
  }

  QDomDocument doc;
  QString parseErrorMsg;
  int parseErrorLine;
  if (!doc.setContent(xml_text, &parseErrorMsg, &parseErrorLine))
  {
    QMessageBox::critical(nullptr, "Error",
                          QString("Failed to parse snippets (xml), error %1 at line %2")
                              .arg(parseErrorMsg)
                              .arg(parseErrorLine));
    return {};
  }
  else
  {
    QDomElement snippets_element = doc.documentElement();
    return GetSnippetsFromXML(snippets_element);
  }
}

SnippetsMap GetSnippetsFromXML(const QDomElement& snippets_element)
{
  SnippetsMap snippets;

  for (auto elem = snippets_element.firstChildElement("snippet"); !elem.isNull();
       elem = elem.nextSiblingElement("snipp"
                                      "et"))
  {
    SnippetData snippet = GetSnippetFromXML(elem);
    snippets.insert({ snippet.alias_name, snippet });
  }
  return snippets;
}

QDomElement ExportSnippets(const SnippetsMap& snippets, QDomDocument& doc)
{
  auto snippets_root = doc.createElement("snippets");

  for (const auto& it : snippets)
  {
    const auto& snippet = it.second;
    auto element = ExportSnippetToXML(snippet, doc);
    snippets_root.appendChild(element);
  }
  return snippets_root;
}

SnippetData GetSnippetFromXML(const QDomElement& element)
{
  SnippetData snippet;
  snippet.linked_source = element.firstChildElement("linked_source").text().trimmed();
  snippet.alias_name = element.attribute("name");
  snippet.global_vars = element.firstChildElement("global").text().trimmed();
  snippet.function = element.firstChildElement("function").text().trimmed();

  auto additional_el = element.firstChildElement("additional_sources");
  if (!additional_el.isNull())
  {
    int count = 1;
    auto tag_name = QString("v%1").arg(count);
    auto source_el = additional_el.firstChildElement(tag_name);
    while (!source_el.isNull())
    {
      snippet.additional_sources.push_back(source_el.text());
      tag_name = QString("v%1").arg(++count);
      source_el = additional_el.firstChildElement(tag_name);
    }
  }
  return snippet;
}

QDomElement ExportSnippetToXML(const SnippetData& snippet, QDomDocument& doc)
{
  auto element = doc.createElement("snippet");

  element.setAttribute("name", snippet.alias_name);

  auto global_el = doc.createElement("global");
  global_el.appendChild(doc.createTextNode(snippet.global_vars));
  element.appendChild(global_el);

  auto equation_el = doc.createElement("function");
  equation_el.appendChild(doc.createTextNode(snippet.function));
  element.appendChild(equation_el);

  auto linked_el = doc.createElement("linked_source");
  linked_el.appendChild(doc.createTextNode(snippet.linked_source));
  element.appendChild(linked_el);

  if (snippet.additional_sources.size() > 0)
  {
    auto sources_el = doc.createElement("additional_sources");

    int count = 1;
    for (QString curve_name : snippet.additional_sources)
    {
      auto tag_name = QString("v%1").arg(count++);
      auto source_el = doc.createElement(tag_name);
      source_el.appendChild(doc.createTextNode(curve_name));
      sources_el.appendChild(source_el);
    }

    element.appendChild(sources_el);
  }

  return element;
}
