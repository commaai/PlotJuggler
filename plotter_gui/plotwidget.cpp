#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QWheelEvent>
#include <QSettings>
#include <QSvgGenerator>
#include <QClipboard>
#include <iostream>
#include <limits>
#include <set>
#include <memory>
#include <QtXml/QDomElement>
#include "qwt_scale_widget.h"
#include "qwt_plot_canvas.h"
#include "qwt_scale_engine.h"
#include "qwt_scale_map.h"
#include "qwt_plot_layout.h"
#include "qwt_scale_draw.h"
#include "qwt_text.h"
#include "plotwidget.h"
#include "qwt_plot_renderer.h"
#include "qwt_series_data.h"
#include "qwt_date_scale_draw.h"
#include "point_series_xy.h"
#include "suggest_dialog.h"
#include "transforms/custom_function.h"
#include "transforms/custom_timeseries.h"

int PlotWidget::global_color_index = 0;

QColor PlotWidget::getColorHint(PlotData* data)
{
  QSettings settings;
  bool remember_color = settings.value("Preferences::remember_color", true).toBool();
  if (data && remember_color && data->getColorHint() != Qt::black)
  {
    return data->getColorHint();
  }
  QColor color;
  bool use_plot_color_index = settings.value("Preferences::use_plot_color_index", false).toBool();
  int index = _curve_list.size();

  if (!use_plot_color_index)
  {
    index = (PlotWidget::global_color_index++);
  }

  // https://matplotlib.org/3.1.1/users/dflt_style_changes.html
  switch (index % 8)
  {
    case 0:
      color = QColor("#1f77b4");
      break;
    case 1:
      color = QColor("#d62728");
      break;
    case 2:
      color = QColor("#1ac938");
      break;
    case 3:
      color = QColor("#ff7f0e");
      break;

    case 4:
      color = QColor("#f14cc1");
      break;
    case 5:
      color = QColor("#9467bd");
      break;
    case 6:
      color = QColor("#17becf");
      break;
    case 7:
      color = QColor("#bcbd22");
      break;
  }
  if (data)
  {
    data->setColorHint(color);
  }

  return color;
}

class TimeScaleDraw : public QwtScaleDraw
{
  virtual QwtText label(double v) const
  {
    QDateTime dt = QDateTime::fromMSecsSinceEpoch((qint64)(v * 1000));
    if (dt.date().year() == 1970 && dt.date().month() == 1 && dt.date().day() == 1)
    {
      return dt.toString("hh:mm:ss.z");
    }
    return dt.toString("hh:mm:ss.z\nyyyy MMM dd");
  }
};

const double MAX_DOUBLE = std::numeric_limits<double>::max() / 2;

static const char* noTransform = "noTransform";
static const char* Derivative1st = "1st Derivative";
static const char* Derivative2nd = "2nd Derivative";
static bool if_xy_plot_failed_show_dialog = true;

static QStringList builtin_trans = { noTransform, Derivative1st, Derivative2nd };

PlotWidget::PlotWidget(PlotDataMapRef& datamap, QWidget* parent)
  : QwtPlot(parent)
  , _zoomer(nullptr)
  , _magnifier(nullptr)
  , _panner1(nullptr)
  , _panner2(nullptr)
  , _tracker(nullptr)
  , _legend(nullptr)
  , _use_date_time_scale(false)
  , _color_index(0)
  , _mapped_data(datamap)
  , _dragging({ DragInfo::NONE, {}, nullptr })
  , _curve_style(QwtPlotCurve::Lines)
  , _time_offset(0.0)
  , _xy_mode(false)
  , _transform_select_dialog(nullptr)
  , _zoom_enabled(true)
  , _keep_aspect_ratio(true)
{
  connect(this, &PlotWidget::curveListChanged, this, [this]() { this->updateMaximumZoomArea(); });

  this->setAcceptDrops(true);

  this->setMinimumWidth(100);
  this->setMinimumHeight(100);

  this->sizePolicy().setHorizontalPolicy(QSizePolicy::Expanding);
  this->sizePolicy().setVerticalPolicy(QSizePolicy::Expanding);

  QwtPlotCanvas* canvas = new QwtPlotCanvas(this);

  canvas->setFrameStyle(QFrame::NoFrame);
  canvas->setPaintAttribute(QwtPlotCanvas::BackingStore, true);

  this->setCanvas(canvas);
  this->setCanvasBackground(Qt::white);

  this->setAxisAutoScale(QwtPlot::yLeft, true);
  this->setAxisAutoScale(QwtPlot::xBottom, true);

  this->axisScaleEngine(QwtPlot::xBottom)->setAttribute(QwtScaleEngine::Floating, true);
  this->plotLayout()->setAlignCanvasToScales(true);

  //--------------------------
  _zoomer = (new PlotZoomer(this->canvas()));
  _magnifier = (new PlotMagnifier(this->canvas()));
  _panner1 = (new QwtPlotPanner(this->canvas()));
  _panner2 = (new QwtPlotPanner(this->canvas()));
  _tracker = (new CurveTracker(this));

  _grid = new QwtPlotGrid();
  _grid->setPen(QPen(Qt::gray, 0.0, Qt::DotLine));

  _zoomer->setRubberBandPen(QColor(Qt::red, 1, Qt::DotLine));
  _zoomer->setTrackerPen(QColor(Qt::green, 1, Qt::DotLine));
  _zoomer->setMousePattern(QwtEventPattern::MouseSelect1, Qt::LeftButton, Qt::NoModifier);
  connect(_zoomer, &PlotZoomer::zoomed, this, &PlotWidget::on_externallyResized);

  _magnifier->setAxisEnabled(xTop, false);
  _magnifier->setAxisEnabled(yRight, false);

  _magnifier->setZoomInKey(Qt::Key_Plus, Qt::ControlModifier);
  _magnifier->setZoomOutKey(Qt::Key_Minus, Qt::ControlModifier);

  // disable right button. keep mouse wheel
  _magnifier->setMouseButton(Qt::NoButton);
  connect(_magnifier, &PlotMagnifier::rescaled, this, [this](QRectF rect) {
    on_externallyResized(rect);
    replot();
  });

  _panner1->setMouseButton(Qt::LeftButton, Qt::ControlModifier);
  _panner2->setMouseButton(Qt::MiddleButton, Qt::NoModifier);

  connect(_panner1, &QwtPlotPanner::panned, this, &PlotWidget::on_panned);
  connect(_panner2, &QwtPlotPanner::panned, this, &PlotWidget::on_panned);

  //-------------------------

  buildActions();

  _legend = new PlotLegend(this);

  this->canvas()->setMouseTracking(true);

  setDefaultRangeX();

  _custom_Y_limits.min = (-MAX_DOUBLE);
  _custom_Y_limits.max = (MAX_DOUBLE);

  QwtScaleWidget* bottomAxis = this->axisWidget(xBottom);
  QwtScaleWidget* leftAxis = this->axisWidget(yLeft);

  bottomAxis->installEventFilter(this);
  leftAxis->installEventFilter(this);
}

void PlotWidget::buildActions()
{
  QIcon iconDeleteList;

  _action_removeAllCurves = new QAction("&Remove ALL curves", this);
  connect(_action_removeAllCurves, &QAction::triggered, this, &PlotWidget::removeAllCurves);
  connect(_action_removeAllCurves, &QAction::triggered, this, &PlotWidget::undoableChange);

  _action_zoomOutMaximum = new QAction("&Zoom Out", this);
  connect(_action_zoomOutMaximum, &QAction::triggered, this, [this]() {
    zoomOut(true);
    replot();
    emit undoableChange();
  });

  _action_zoomOutHorizontally = new QAction("&Zoom Out Horizontally", this);
  connect(_action_zoomOutHorizontally, &QAction::triggered, this, [this]() {
    on_zoomOutHorizontal_triggered(true);
    replot();
    emit undoableChange();
  });

  _action_zoomOutVertically = new QAction("&Zoom Out Vertically", this);
  connect(_action_zoomOutVertically, &QAction::triggered, this, [this]() {
    on_zoomOutVertical_triggered(true);
    replot();
    emit undoableChange();
  });

  QFont font;
  font.setPointSize(10);

  _action_saveToFile = new QAction("&Save plot to file", this);
  connect(_action_saveToFile, &QAction::triggered, this, &PlotWidget::on_savePlotToFile);

  _action_clipboard = new QAction("&Copy to clipboard", this);
  connect(_action_clipboard, &QAction::triggered, this, &PlotWidget::on_copyToClipboard);

}

void PlotWidget::canvasContextMenuTriggered(const QPoint& pos)
{
  QSettings settings;
  QString theme = settings.value("Preferences::theme", "style_light").toString();

  auto setIcon = [&](QAction* action, QString file) {
    QIcon icon;
    icon.addFile(tr(":/%1/%2").arg(theme).arg(file), QSize(24, 24));
    action->setIcon(icon);
  };

  setIcon(_action_removeAllCurves, "remove.png");
  setIcon(_action_zoomOutMaximum, "zoom_max.png");
  setIcon(_action_zoomOutHorizontally, "zoom_horizontal.png");
  setIcon(_action_zoomOutVertically, "zoom_vertical.png");
  setIcon(_action_clipboard, "copy_clipboard.png");
  setIcon(_action_saveToFile, "save.png");

  QMenu menu(this);
  menu.addAction(_action_removeAllCurves);
  menu.addSeparator();
  menu.addAction(_action_zoomOutMaximum);
  menu.addAction(_action_zoomOutHorizontally);
  menu.addAction(_action_zoomOutVertically);
  menu.addSeparator();
  menu.addSeparator();
  menu.addAction(_action_clipboard);
  menu.addAction(_action_saveToFile);

  _action_removeAllCurves->setEnabled(!_curve_list.empty());

  menu.exec(canvas()->mapToGlobal(pos));
}

PlotWidget::~PlotWidget()
{
}

bool PlotWidget::addCurve(const std::string& name)
{
  auto it = _mapped_data.numeric.find(name);
  if (it == _mapped_data.numeric.end())
  {
    return false;
  }

  if (_curve_list.find(name) != _curve_list.end())
  {
    return false;
  }

  PlotData& data = it->second;
  const auto qname = QString::fromStdString(name);

  auto curve = new QwtPlotCurve(qname);
  try
  {
    auto plot_qwt = createTimeSeries("", &data);

    curve->setPaintAttribute(QwtPlotCurve::ClipPolygons, true);
    curve->setPaintAttribute(QwtPlotCurve::FilterPointsAggressive, true);
    curve->setData(plot_qwt);
  }
  catch (std::exception& ex)
  {
    QMessageBox::warning(this, "Exception!", ex.what());
    return false;
  }

  QColor color = getColorHint(&data);
  curve->setPen(color, (_curve_style == QwtPlotCurve::Dots) ? 4.0 : 1.3);
  curve->setStyle(_curve_style);

  curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);

  curve->attach(this);
  _curve_list.insert(std::make_pair(name, curve));

  auto marker = new QwtPlotMarker;
  _point_marker.insert(std::make_pair(name, marker));
  marker->attach(this);
  marker->setVisible(isXYPlot());

  QwtSymbol* sym = new QwtSymbol(QwtSymbol::Ellipse, Qt::red, QPen(Qt::black), QSize(8, 8));

  marker->setSymbol(sym);

  return true;
}

bool PlotWidget::addCurveXY(std::string name_x, std::string name_y, QString curve_name)
{
  std::string name = curve_name.toStdString();

  while (name.empty())
  {
    SuggestDialog dialog(name_x, name_y, this);

    bool ok = (dialog.exec() == QDialog::Accepted);
    QString text = dialog.suggestedName();
    name = text.toStdString();
    name_x = dialog.nameX().toStdString();
    name_y = dialog.nameY().toStdString();

    if (!ok || name.empty() || _curve_list.count(name) != 0)
    {
      int ret = QMessageBox::warning(this, "Missing name", "The name is missing or invalid. Try again or abort.",
                                     QMessageBox::Abort | QMessageBox::Retry, QMessageBox::Retry);
      if (ret == QMessageBox::Abort)
      {
        return false;
      }
      name.clear();
    }
  }

  auto it = _mapped_data.numeric.find(name_x);
  if (it == _mapped_data.numeric.end())
  {
    throw std::runtime_error("Creation of XY plot failed");
  }
  PlotData& data_x = it->second;

  it = _mapped_data.numeric.find(name_y);
  if (it == _mapped_data.numeric.end())
  {
    throw std::runtime_error("Creation of XY plot failed");
  }
  PlotData& data_y = it->second;

  if (_curve_list.find(name) != _curve_list.end())
  {
    return false;
  }

  const auto qname = QString::fromStdString(name);

  auto curve = new QwtPlotCurve(qname);
  try
  {
    auto plot_qwt = createCurveXY(&data_x, &data_y);

    curve->setPaintAttribute(QwtPlotCurve::ClipPolygons, true);
    curve->setPaintAttribute(QwtPlotCurve::FilterPointsAggressive, true);
    curve->setData(plot_qwt);
  }
  catch (std::exception& ex)
  {
    QMessageBox::warning(this, "Exception!", ex.what());
    return false;
  }


  QColor color = getColorHint(nullptr);

  curve->setPen(color, (_curve_style == QwtPlotCurve::Dots) ? 4.0 : 1.3);
  curve->setStyle(_curve_style);

  curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);

  curve->attach(this);
  _curve_list.insert(std::make_pair(name, curve));

  auto marker = new QwtPlotMarker;
  _point_marker.insert(std::make_pair(name, marker));
  marker->attach(this);
  marker->setVisible(isXYPlot());

  QwtSymbol* sym = new QwtSymbol(QwtSymbol::Ellipse, Qt::red, QPen(Qt::black), QSize(8, 8));

  marker->setSymbol(sym);

  return true;
}

void PlotWidget::removeCurve(const std::string& curve_name)
{
  bool deleted = false;

  for (auto it = _curve_list.begin(); it != _curve_list.end();)
  {
    PointSeriesXY* curve_xy = dynamic_cast<PointSeriesXY*>(it->second->data());
    bool remove_curve_xy =
        curve_xy && (curve_xy->dataX()->name() == curve_name || curve_xy->dataY()->name() == curve_name);

    if (it->first == curve_name || remove_curve_xy)
    {
      deleted = true;
      auto& curve = it->second;
      curve->detach();

      auto marker_it = _point_marker.find(it->first);
      if (marker_it != _point_marker.end())
      {
        auto marker = marker_it->second;
        if (marker)
        {
          marker->detach();
        }
        _point_marker.erase(marker_it);
      }

      it = _curve_list.erase(it);
    }
    else
    {
      it++;
    }
  }

  if (deleted)
  {
    _tracker->redraw();
    emit curveListChanged();
  }
}

bool PlotWidget::isEmpty() const
{
  return _curve_list.empty();
}

const std::map<std::string, QwtPlotCurve*>& PlotWidget::curveList() const
{
  return _curve_list;
}

void PlotWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const QMimeData* mimeData = event->mimeData();
  QStringList mimeFormats = mimeData->formats();
  _dragging.curves.clear();
  _dragging.source = event->source();

  for (const QString& format : mimeFormats)
  {
    QByteArray encoded = mimeData->data(format);
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    while (!stream.atEnd())
    {
      QString curve_name;
      stream >> curve_name;
      if (!curve_name.isEmpty())
      {
        _dragging.curves.push_back(curve_name);
      }
    }

    if (format == "curveslist/add_curve")
    {
      _dragging.mode = DragInfo::CURVES;
      event->acceptProposedAction();
    }
    if (format == "curveslist/new_XY_axis")
    {
      if (_dragging.curves.size() != 2)
      {
        qDebug() << "FATAL: Dragging " << _dragging.curves.size() << " curves";
        return;
      }
      if( _curve_list.empty() || isXYPlot())
      {
        _dragging.mode = DragInfo::NEW_XY;
        event->acceptProposedAction();
      }
      else{
        event->ignore();
      }
    }
  }
}

void PlotWidget::dragLeaveEvent(QDragLeaveEvent*)
{
  _dragging.mode = DragInfo::NONE;
  _dragging.curves.clear();
}

void PlotWidget::dropEvent(QDropEvent*)
{
  bool curves_changed = false;

  if (_dragging.mode == DragInfo::CURVES)
  {
    if ( isXYPlot() && !_curve_list.empty())
    {
      _dragging.mode = DragInfo::NONE;
      _dragging.curves.clear();
      QMessageBox::warning(this, "Warning",
                           tr("This is a XY plot, you can not drop normal time series here.\n"
                              "Clear all curves to reset it to normal mode."));
      return;
    }
    else if ( isXYPlot() && _curve_list.empty())
    {
      setModeXY(false);
    }

    for (const auto& curve_name : _dragging.curves)
    {
      bool added = addCurve(curve_name.toStdString());
      curves_changed = curves_changed || added;
    }
  }
  else if (_dragging.mode == DragInfo::NEW_XY && _dragging.curves.size() == 2)
  {
    if (!_curve_list.empty() && !_xy_mode)
    {
      _dragging.mode = DragInfo::NONE;
      _dragging.curves.clear();
      QMessageBox::warning(this, "Warning",
                           tr("To convert this widget into a XY plot, "
                              "you must first remove all the time series."));
      return;
    }

    setModeXY(true);
    addCurveXY(_dragging.curves[0].toStdString(),
               _dragging.curves[1].toStdString());

    curves_changed = true;
  }

  if (curves_changed)
  {
    emit curvesDropped();
    emit curveListChanged();
    zoomOut(true);
  }
  _dragging.mode = DragInfo::NONE;
  _dragging.curves.clear();
}

void PlotWidget::removeAllCurves()
{
  for (auto& it : _curve_list)
  {
    it.second->detach();
  }
  for (auto& it : _point_marker)
  {
    it.second->detach();
  }

  setModeXY(false);
  _curve_list.clear();
  _point_marker.clear();
  _tracker->redraw();

  emit curveListChanged();

  replot();
}

void PlotWidget::on_panned(int, int)
{
  on_externallyResized(canvasBoundingRect());
}

QDomElement PlotWidget::xmlSaveState(QDomDocument& doc) const
{
  QDomElement plot_el = doc.createElement("plot");

  QDomElement range_el = doc.createElement("range");
  QRectF rect = this->canvasBoundingRect();
  range_el.setAttribute("bottom", QString::number(rect.bottom(), 'f', 6));
  range_el.setAttribute("top", QString::number(rect.top(), 'f', 6));
  range_el.setAttribute("left", QString::number(rect.left(), 'f', 6));
  range_el.setAttribute("right", QString::number(rect.right(), 'f', 6));
  plot_el.appendChild(range_el);

  QDomElement limitY_el = doc.createElement("limitY");
  if (_custom_Y_limits.min > -MAX_DOUBLE)
  {
    limitY_el.setAttribute("min", QString::number(_custom_Y_limits.min));
  }
  if (_custom_Y_limits.max < MAX_DOUBLE)
  {
    limitY_el.setAttribute("max", QString::number(_custom_Y_limits.max));
  }
  plot_el.appendChild(limitY_el);

  if (_curve_style == QwtPlotCurve::Lines)
  {
    plot_el.setAttribute("style", "Lines");
  }
  else if (_curve_style == QwtPlotCurve::LinesAndDots)
  {
    plot_el.setAttribute("style", "LinesAndDots");
  }
  else if (_curve_style == QwtPlotCurve::Dots)
  {
    plot_el.setAttribute("style", "Dots");
  }

  for (auto& it : _curve_list)
  {
    auto& name = it.first;
    QwtPlotCurve* curve = it.second;
    QDomElement curve_el = doc.createElement("curve");
    curve_el.setAttribute("name", QString::fromStdString(name));
    curve_el.setAttribute("color", curve->pen().color().name());

    plot_el.appendChild(curve_el);

    if (_xy_mode)
    {
      PointSeriesXY* curve_xy = dynamic_cast<PointSeriesXY*>(curve->data());
      curve_el.setAttribute("curve_x", QString::fromStdString(curve_xy->dataX()->name()));
      curve_el.setAttribute("curve_y", QString::fromStdString(curve_xy->dataY()->name()));
    }
    else{
      TimeSeries* ts = dynamic_cast<TimeSeries*>(curve->data());
      if(ts && ts->transform())
      {
        QDomElement transform_el = doc.createElement("transform");
        transform_el.setAttribute("name", ts->transformName() );
        ts->transform()->xmlSaveState(doc, transform_el);
        curve_el.appendChild(transform_el);
      }
    }
  }

  return plot_el;
}

bool PlotWidget::xmlLoadState(QDomElement& plot_widget)
{
  std::set<std::string> added_curve_names;

  QString mode = plot_widget.attribute("mode");
  setModeXY(mode == "XYPlot");

  QDomElement limitY_el = plot_widget.firstChildElement("limitY");

  _custom_Y_limits.min = -MAX_DOUBLE;
  _custom_Y_limits.max = +MAX_DOUBLE;

  if (!limitY_el.isNull())
  {
    if (limitY_el.hasAttribute("min"))
    {
      _custom_Y_limits.min = limitY_el.attribute("min").toDouble();
    }
    if (limitY_el.hasAttribute("max"))
    {
      _custom_Y_limits.max = limitY_el.attribute("max").toDouble();
    }
  }

  static bool warning_message_shown = false;

  bool curve_added = false;

  for (QDomElement curve_element = plot_widget.firstChildElement("curve"); !curve_element.isNull();
       curve_element = curve_element.nextSiblingElement("curve"))
  {
    QString curve_name = curve_element.attribute("name");
    std::string curve_name_std = curve_name.toStdString();
    QColor color( curve_element.attribute("color"));

    bool error = false;
    if (!isXYPlot())
    {
      if (_mapped_data.numeric.find(curve_name_std) == _mapped_data.numeric.end())
      {
        error = true;
      }
      else
      {
        auto added = addCurve(curve_name_std);
        curve_added = curve_added || added;
        auto& curve = _curve_list[curve_name_std];
        curve->setPen(color, 1.3);
        added_curve_names.insert(curve_name_std);

        TimeSeries* ts = dynamic_cast<TimeSeries*>(curve->data());
        QDomElement transform_el = curve_element.firstChildElement("transform");
        if( transform_el.isNull() == false )
        {
          ts->setTransform( transform_el.attribute("name") );
          ts->transform()->xmlLoadState(transform_el);
          ts->updateCache();
        }
      }
    }
    else
    {
      std::string curve_x = curve_element.attribute("curve_x").toStdString();
      std::string curve_y = curve_element.attribute("curve_y").toStdString();

      if (_mapped_data.numeric.find(curve_x) == _mapped_data.numeric.end() ||
          _mapped_data.numeric.find(curve_y) == _mapped_data.numeric.end())
      {
        error = true;
      }
      else
      {
        auto added = addCurveXY(curve_x, curve_y, curve_name);
        curve_added = curve_added || added;
        _curve_list[curve_name_std]->setPen(color, 1.3);
        added_curve_names.insert(curve_name_std);
      }
    }

    if (error && !warning_message_shown)
    {
      QMessageBox::warning(this, "Warning",
                           tr("Can't find one or more curves.\n"
                              "This message will be shown only once."));
      warning_message_shown = true;
    }
  }

  bool curve_removed = true;

  while (curve_removed)
  {
    curve_removed = false;
    for (auto& it : _curve_list)
    {
      auto curve_name = it.first;
      if (added_curve_names.find(curve_name) == added_curve_names.end())
      {
        removeCurve(curve_name);
        curve_removed = true;
        break;
      }
    }
  }

  if (curve_removed || curve_added)
  {
    _tracker->redraw();
    // replot();
    emit curveListChanged();
  }

  //-----------------------------------------

  QDomElement rectangle = plot_widget.firstChildElement("range");

  if (!rectangle.isNull())
  {
    QRectF rect;
    rect.setBottom(rectangle.attribute("bottom").toDouble());
    rect.setTop(rectangle.attribute("top").toDouble());
    rect.setLeft(rectangle.attribute("left").toDouble());
    rect.setRight(rectangle.attribute("right").toDouble());
    this->setZoomRectangle(rect, false);
  }

  if (plot_widget.hasAttribute("style"))
  {
    QString style = plot_widget.attribute("style");
    if (style == "Lines")
    {
      _curve_style = QwtPlotCurve::Lines;
    }
    else if (style == "LinesAndDots")
    {
      _curve_style = QwtPlotCurve::LinesAndDots;
    }
    else if (style == "Dots")
    {
      _curve_style = QwtPlotCurve::Dots;
    }
    changeCurveStyle(_curve_style);
  }

  updateMaximumZoomArea();
  replot();
  return true;
}

QRectF PlotWidget::canvasBoundingRect() const
{
  QRectF rect;
  rect.setBottom(this->canvasMap(yLeft).s1());
  rect.setTop(this->canvasMap(yLeft).s2());
  rect.setLeft(this->canvasMap(xBottom).s1());
  rect.setRight(this->canvasMap(xBottom).s2());
  return rect;
}

void PlotWidget::updateMaximumZoomArea()
{
  QRectF max_rect;
  auto rangeX = getMaximumRangeX();
  max_rect.setLeft(rangeX.min);
  max_rect.setRight(rangeX.max);

  auto rangeY = getMaximumRangeY(rangeX);
  max_rect.setBottom(rangeY.min);
  max_rect.setTop(rangeY.max);

  if (isXYPlot() && _keep_aspect_ratio)
  {
    const QRectF canvas_rect = canvas()->contentsRect();
    const double canvas_ratio = fabs(canvas_rect.width() / canvas_rect.height());
    const double data_ratio = fabs(max_rect.width() / max_rect.height());
    if (data_ratio < canvas_ratio)
    {
      // height is negative!!!!
      double new_width = fabs(max_rect.height() * canvas_ratio);
      double increment = new_width - max_rect.width();
      max_rect.setWidth(new_width);
      max_rect.moveLeft(max_rect.left() - 0.5 * increment);
    }
    else
    {
      // height must be negative!!!!
      double new_height = -(max_rect.width() / canvas_ratio);
      double increment = fabs(new_height - max_rect.height());
      max_rect.setHeight(new_height);
      max_rect.moveTop(max_rect.top() + 0.5 * increment);
    }
    _magnifier->setAxisLimits(xBottom, max_rect.left(), max_rect.right());
    _magnifier->setAxisLimits(yLeft, max_rect.bottom(), max_rect.top());
    _zoomer->keepAspectratio(true);
  }
  else
  {
    _magnifier->setAxisLimits(xBottom, max_rect.left(), max_rect.right());
    _magnifier->setAxisLimits(yLeft, max_rect.bottom(), max_rect.top());
    _zoomer->keepAspectratio(false);
  }
  _max_zoom_rect = max_rect;
}

void PlotWidget::rescaleEqualAxisScaling()
{
  const QwtScaleMap xMap = canvasMap(QwtPlot::xBottom);
  const QwtScaleMap yMap = canvasMap(QwtPlot::yLeft);

  QRectF canvas_rect = canvas()->contentsRect();
  canvas_rect = canvas_rect.normalized();
  const double x1 = xMap.invTransform(canvas_rect.left());
  const double x2 = xMap.invTransform(canvas_rect.right());
  const double y1 = yMap.invTransform(canvas_rect.bottom());
  const double y2 = yMap.invTransform(canvas_rect.top());

  const double data_ratio = (x2 - x1) / (y2 - y1);
  const double canvas_ratio = canvas_rect.width() / canvas_rect.height();
  const double max_ratio = fabs(_max_zoom_rect.width() / _max_zoom_rect.height());

  QRectF rect(QPointF(x1, y2), QPointF(x2, y1));

  if (data_ratio < canvas_ratio)
  {
    double new_width = fabs(rect.height() * canvas_ratio);
    double increment = new_width - rect.width();
    rect.setWidth(new_width);
    rect.moveLeft(rect.left() - 0.5 * increment);
  }
  else
  {
    double new_height = -(rect.width() / canvas_ratio);
    double increment = fabs(new_height - rect.height());
    rect.setHeight(new_height);
    rect.moveTop(rect.top() + 0.5 * increment);
  }
  if (rect.contains(_max_zoom_rect))
  {
    rect = _max_zoom_rect;
  }

  this->setAxisScale(yLeft, std::min(rect.bottom(), rect.top()), std::max(rect.bottom(), rect.top()));
  this->setAxisScale(xBottom, std::min(rect.left(), rect.right()), std::max(rect.left(), rect.right()));
  this->updateAxes();
}

void PlotWidget::resizeEvent(QResizeEvent* ev)
{
  QwtPlot::resizeEvent(ev);
  updateMaximumZoomArea();

  if (isXYPlot() && _keep_aspect_ratio)
  {
    rescaleEqualAxisScaling();
  }
}

void PlotWidget::updateLayout()
{
  QwtPlot::updateLayout();
  // qDebug() << canvasBoundingRect();
}

void PlotWidget::setConstantRatioXY(bool active)
{
  _keep_aspect_ratio = active;
  if (isXYPlot() && active)
  {
    _zoomer->keepAspectratio(true);
  }
  else
  {
    _zoomer->keepAspectratio(false);
  }
  zoomOut(false);
}

void PlotWidget::setZoomRectangle(QRectF rect, bool emit_signal)
{
  QRectF current_rect = canvasBoundingRect();
  if (current_rect == rect)
  {
    return;
  }
  this->setAxisScale(yLeft, std::min(rect.bottom(), rect.top()), std::max(rect.bottom(), rect.top()));
  this->setAxisScale(xBottom, std::min(rect.left(), rect.right()), std::max(rect.left(), rect.right()));
  this->updateAxes();

  if (isXYPlot() && _keep_aspect_ratio)
  {
    rescaleEqualAxisScaling();
  }

  if (emit_signal)
  {
    if (isXYPlot())
    {
      emit undoableChange();
    }
    else
    {
      emit rectChanged(this, rect);
    }
  }
}

void PlotWidget::reloadPlotData()
{
  // TODO: this needs MUCH more testing

  int visible = 0;

  for (auto& curve_it : _curve_list)
  {
    if (curve_it.second->isVisible()){
      visible++;
    }

    auto& curve = curve_it.second;
    const auto& curve_name = curve_it.first;

    auto data_it = _mapped_data.numeric.find(curve_name);
    if (data_it != _mapped_data.numeric.end())
    {
      const auto& data = data_it->second;
      QString transform_name;
      if( !isXYPlot() )
      {
        TimeSeries* ts = dynamic_cast<TimeSeries*>(curve->data());
        transform_name = ts->transformName();
      }
      auto data_series = createTimeSeries(transform_name, &data);
      curve->setData(data_series);
    }
  }

  if (_curve_list.size() == 0 || visible == 0)
  {
    setDefaultRangeX();
  }
}

void PlotWidget::activateLegend(bool activate)
{
  _legend->setVisible(activate);
}

void PlotWidget::activateGrid(bool activate)
{
  _grid->enableX(activate);
  _grid->enableXMin(activate);
  _grid->enableY(activate);
  _grid->enableYMin(activate);
  _grid->attach(this);
}

void PlotWidget::configureTracker(CurveTracker::Parameter val)
{
  _tracker->setParameter(val);
}

void PlotWidget::enableTracker(bool enable)
{
  _tracker->setEnabled(enable && !isXYPlot());
}

bool PlotWidget::isTrackerEnabled() const
{
  return _tracker->isEnabled();
}

void PlotWidget::setTrackerPosition(double abs_time)
{
  if (isXYPlot())
  {
    for (auto& it : _curve_list)
    {
      auto& name = it.first;
      auto series = static_cast<DataSeriesBase*>(it.second->data());
      auto pointXY = series->sampleFromTime(abs_time);
      if (pointXY)
      {
        _point_marker[name]->setValue(pointXY.value());
      }
    }
  }
  else
  {
    double relative_time = abs_time - _time_offset;
    _tracker->setPosition(QPointF(relative_time, 0.0));
  }
}

void PlotWidget::on_changeTimeOffset(double offset)
{
  auto prev_offset = _time_offset;
  _time_offset = offset;

  if (fabs(prev_offset - offset) > std::numeric_limits<double>::epsilon())
  {
    for (auto& it : _curve_list)
    {
      auto series = static_cast<DataSeriesBase*>(it.second->data());
      series->setTimeOffset(_time_offset);
    }
    if (!isXYPlot())
    {
      QRectF rect = canvasBoundingRect();
      double delta = prev_offset - offset;
      rect.moveLeft(rect.left() + delta);
      setZoomRectangle(rect, false);
    }
  }
}

void PlotWidget::on_changeDateTimeScale(bool enable)
{
  _use_date_time_scale = enable;
  bool is_timescale = dynamic_cast<TimeScaleDraw*>(axisScaleDraw(QwtPlot::xBottom)) != nullptr;

  if (enable && !isXYPlot())
  {
    if (!is_timescale)
    {
      setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw());
    }
  }
  else
  {
    if (is_timescale)
    {
      setAxisScaleDraw(QwtPlot::xBottom, new QwtScaleDraw);
    }
  }
}

PlotData::RangeTime PlotWidget::getMaximumRangeX() const
{
  double left = std::numeric_limits<double>::max();
  double right = -std::numeric_limits<double>::max();

  for (auto& it : _curve_list)
  {
    if (!it.second->isVisible())
      continue;

    auto series = static_cast<DataSeriesBase*>(it.second->data());
    const auto max_range_X = series->getVisualizationRangeX();
    if (!max_range_X)
      continue;

    left = std::min(max_range_X->min, left);
    right = std::max(max_range_X->max, right);
  }

  if (left > right)
  {
    left = 0;
    right = 0;
  }

  double margin = 0.0;
  if (fabs(right - left) > std::numeric_limits<double>::epsilon())
  {
    margin = isXYPlot() ? ((right - left) * 0.025) : 0.0;
  }
  right = right + margin;
  left = left - margin;

  return PlotData::RangeTime({ left, right });
}

// TODO report failure for empty dataset
PlotData::RangeValue PlotWidget::getMaximumRangeY(PlotData::RangeTime range_X) const
{
  double top = -std::numeric_limits<double>::max();
  double bottom = std::numeric_limits<double>::max();

  for (auto& it : _curve_list)
  {
    if (!it.second->isVisible())
      continue;

    auto series = static_cast<DataSeriesBase*>(it.second->data());

    const auto max_range_X = series->getVisualizationRangeX();
    if (!max_range_X){
      continue;
    }

    double left = std::max(max_range_X->min, range_X.min);
    double right = std::min(max_range_X->max, range_X.max);

    left += _time_offset;
    right += _time_offset;
    left = std::nextafter(left, right);
    right = std::nextafter(right, left);

    auto range_Y = series->getVisualizationRangeY({ left, right });
    if (!range_Y)
    {
      qDebug() << " invalid range_Y in PlotWidget::maximumRangeY";
      continue;
    }
    if (top < range_Y->max){
      top = range_Y->max;
    }
    if (bottom > range_Y->min){
      bottom = range_Y->min;
    }
  }

  double margin = 0.1;

  if (bottom > top)
  {
    bottom = 0;
    top = 0;
  }

  if (top - bottom > std::numeric_limits<double>::epsilon())
  {
    margin = (top - bottom) * 0.025;
  }

  const bool lower_limit = _custom_Y_limits.min > -MAX_DOUBLE;
  const bool upper_limit = _custom_Y_limits.max < MAX_DOUBLE;

  if (lower_limit)
  {
    bottom = _custom_Y_limits.min;
    if (top < bottom){
      top = bottom + margin;
    }
  }

  if (upper_limit)
  {
    top = _custom_Y_limits.max;
    if (top < bottom){
      bottom = top - margin;
    }
  }

  if (!lower_limit && !upper_limit)
  {
    top += margin;
    bottom -= margin;
  }

  return PlotData::RangeValue({ bottom, top });
}

void PlotWidget::updateCurves()
{
  for (auto& it : _curve_list)
  {
    auto series = static_cast<DataSeriesBase*>(it.second->data());
    bool res = series->updateCache();
    // TODO check res and do something if false.
  }
}

std::map<std::string, QColor> PlotWidget::getCurveColors() const
{
  std::map<std::string, QColor> color_by_name;

  for (auto& it : _curve_list)
  {
    const auto& curve_name = it.first;
    auto& curve = it.second;
    color_by_name.insert( {curve_name, curve->pen().color()} );
  }
  return color_by_name;
}

void PlotWidget::on_changeCurveColor(const std::string& curve_name, QColor new_color)
{
  auto it = _curve_list.find(curve_name);
  if (it != _curve_list.end())
  {
    auto& curve = it->second;
    if (curve->pen().color() != new_color)
    {
      curve->setPen(new_color, 1.3);
    }
    replot();
  }
}

void PlotWidget::changeCurveStyle(QwtPlotCurve::CurveStyle style)
{
  _curve_style = style;
  for (auto& it : _curve_list)
  {
    auto& curve = it.second;
    curve->setPen(curve->pen().color(), (_curve_style == QwtPlotCurve::Dots) ? 4.0 : 1.3);
    curve->setStyle(_curve_style);
  }
  replot();
}

void PlotWidget::on_showPoints_triggered()
{
  if (_curve_style == QwtPlotCurve::Lines)
  {
    _curve_style = QwtPlotCurve::LinesAndDots;
  }
  else if (_curve_style == QwtPlotCurve::LinesAndDots)
  {
    _curve_style = QwtPlotCurve::Dots;
  }
  else if (_curve_style == QwtPlotCurve::Dots)
  {
    _curve_style = QwtPlotCurve::Lines;
  }

  changeCurveStyle(_curve_style);
}

void PlotWidget::on_externallyResized(const QRectF& rect)
{
  QRectF current_rect = canvasBoundingRect();
  if (current_rect == rect)
  {
    return;
  }

  if (isXYPlot())
  {
    emit undoableChange();
  }
  else
  {
    emit rectChanged(this, rect);
  }
}

void PlotWidget::zoomOut(bool emit_signal)
{
  if (_curve_list.size() == 0)
  {
    QRectF rect(0, 1, 1, -1);
    this->setZoomRectangle(rect, false);
    return;
  }
  updateMaximumZoomArea();
  setZoomRectangle(_max_zoom_rect, emit_signal);
  replot();
}

void PlotWidget::on_zoomOutHorizontal_triggered(bool emit_signal)
{
  updateMaximumZoomArea();
  QRectF act = canvasBoundingRect();
  auto rangeX = getMaximumRangeX();

  act.setLeft(rangeX.min);
  act.setRight(rangeX.max);
  this->setZoomRectangle(act, emit_signal);
}

void PlotWidget::on_zoomOutVertical_triggered(bool emit_signal)
{
  updateMaximumZoomArea();
  QRectF rect = canvasBoundingRect();
  auto rangeY = getMaximumRangeY({ rect.left(), rect.right() });

  rect.setBottom(rangeY.min);
  rect.setTop(rangeY.max);
  this->setZoomRectangle(rect, emit_signal);
}

//void PlotWidget::on_changeToBuiltinTransforms(QString new_transform)
//{
//  _xy_mode = false;

//  enableTracker(true);

//  for (auto& it : _curve_list)
//  {
//    const auto& curve_name = it.first;
//    auto& curve = it.second;

//    _point_marker[curve_name]->setVisible(false);
//    curve->setTitle(QString::fromStdString(curve_name));
//    _curves_transform[curve_name] = new_transform;

//    auto data_it = _mapped_data.numeric.find(curve_name);
//    if (data_it != _mapped_data.numeric.end())
//    {
//      const auto& data = data_it->second;
//      auto data_series = createTimeSeries(new_transform, &data);
//      curve->setData(data_series);
//    }
//  }

//  _default_transform = new_transform;
//  zoomOut(true);
//  on_changeDateTimeScale(_use_date_time_scale);
//  replot();
//}

bool PlotWidget::isXYPlot() const
{
  return _xy_mode;
}

void PlotWidget::setModeXY(bool enable)
{
  if( enable == _xy_mode)
  {
    return;
  }
  _xy_mode = enable;

  enableTracker(!enable);

  if( enable ){
    QFont font_footer;
    font_footer.setPointSize(10);
    QwtText text("XY Plot");
    text.setFont(font_footer);
    this->setFooter(text);
  }
  else{
    this->setFooter("");
  }

  zoomOut(true);
  on_changeDateTimeScale(_use_date_time_scale);
  replot();
}

void PlotWidget::updateAvailableTransformers()
{
  QSettings settings;
  QByteArray xml_text = settings.value("AddCustomPlotDialog.savedXML", QByteArray()).toByteArray();
  if (!xml_text.isEmpty())
  {
    _snippets = GetSnippetsFromXML(xml_text);
  }
}

void PlotWidget::transformCustomCurves()
{
  std::string error_message;

  /*for (auto& curve_it : _curve_list)
  {
    auto& curve = curve_it.second;
    const auto& curve_name = curve_it.first;
    const auto& transform = _curves_transform.at(curve_name);

    auto data_it = _mapped_data.numeric.find(curve_name);
    if (data_it != _mapped_data.numeric.end())
    {
      auto& data = data_it->second;
      try
      {
        auto data_series = createTimeSeries(transform, &data);
        curve->setData(data_series);

        if (transform == noTransform || transform.isEmpty())
        {
          curve->setTitle(QString::fromStdString(curve_name));
        }
        else
        {
          curve->setTitle(QString::fromStdString(curve_name) + tr(" [") + transform + tr("]"));
        }
      }
      catch (std::runtime_error& err)
      {
        auto data_series = createTimeSeries(noTransform, &data);
        curve->setData(data_series);

        error_message += curve_name + (" [") + transform.toStdString() + ("]: ");
        error_message += err.what();

        curve->setTitle(QString::fromStdString(curve_name));
      }
    }
  }
  if (error_message.size() > 0)
  {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Warnings");
    msgBox.setText(tr("Something wrong happened while creating the following curves. "
                      "Please check that the transform equation is correct.\n\n") +
                   QString::fromStdString(error_message));
    msgBox.exec();
  }*/
}

void PlotWidget::on_savePlotToFile()
{
  QString fileName;

  QFileDialog saveDialog(this);
  saveDialog.setAcceptMode(QFileDialog::AcceptSave);

  QStringList filters;
  filters << "png (*.png)"
          << "jpg (*.jpg *.jpeg)"
          << "svg (*.svg)";

  saveDialog.setNameFilters(filters);
  saveDialog.exec();

  if (saveDialog.result() == QDialog::Accepted && !saveDialog.selectedFiles().empty())
  {
    fileName = saveDialog.selectedFiles().first();

    if (fileName.isEmpty())
    {
      return;
    }

    bool is_svg = false;
    QFileInfo fileinfo(fileName);
    if (fileinfo.suffix().isEmpty())
    {
      auto filter = saveDialog.selectedNameFilter();
      if (filter == filters[0])
      {
        fileName.append(".png");
      }
      else if (filter == filters[1])
      {
        fileName.append(".jpg");
      }
      else if (filter == filters[2])
      {
        fileName.append(".svg");
        is_svg = true;
      }
    }

    bool tracker_enabled = _tracker->isEnabled();
    if (tracker_enabled)
    {
      this->enableTracker(false);
      replot();
    }

    QRect documentRect(0, 0, 1200, 900);
    QwtPlotRenderer rend;

    if (is_svg)
    {
      QSvgGenerator generator;
      generator.setFileName(fileName);
      generator.setResolution(80);
      generator.setViewBox(documentRect);
      QPainter painter(&generator);
      rend.render(this, &painter, documentRect);
    }
    else
    {
      QPixmap pixmap(1200, 900);
      QPainter painter(&pixmap);
      rend.render(this, &painter, documentRect);
      pixmap.save(fileName);
    }

    if (tracker_enabled)
    {
      this->enableTracker(true);
      replot();
    }
  }
}

void PlotWidget::setCustomAxisLimits(PlotData::RangeValue range)
{
  _custom_Y_limits = range;
  on_zoomOutVertical_triggered(false);
  replot();
}

PlotData::RangeValue PlotWidget::customAxisLimit() const
{
  return _custom_Y_limits;
}

void PlotWidget::on_copyToClipboard()
{
  bool tracker_enabled = _tracker->isEnabled();
  if (tracker_enabled)
  {
    this->enableTracker(false);
    replot();
  }

  auto documentRect = this->canvas()->rect();
  qDebug() << documentRect;

  QwtPlotRenderer rend;
  QPixmap pixmap(documentRect.width(), documentRect.height());
  QPainter painter(&pixmap);
  rend.render(this, &painter, documentRect);

  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setPixmap(pixmap);

  if (tracker_enabled)
  {
    this->enableTracker(true);
    replot();
  }
}

bool PlotWidget::eventFilter(QObject* obj, QEvent* event)
{
  QwtScaleWidget* bottomAxis = this->axisWidget(xBottom);
  QwtScaleWidget* leftAxis = this->axisWidget(yLeft);

  if (_magnifier && (obj == bottomAxis || obj == leftAxis) && !(isXYPlot() && _keep_aspect_ratio))
  {
    if (event->type() == QEvent::Wheel)
    {
      auto wheel_event = dynamic_cast<QWheelEvent*>(event);
      if (obj == bottomAxis)
      {
        _magnifier->setDefaultMode(PlotMagnifier::X_AXIS);
      }
      else
      {
        _magnifier->setDefaultMode(PlotMagnifier::Y_AXIS);
      }
      _magnifier->widgetWheelEvent(wheel_event);
    }
  }

  if (obj == canvas())
  {
    if (_magnifier)
    {
      _magnifier->setDefaultMode(PlotMagnifier::BOTH_AXES);
    }
    return canvasEventFilter(event);
  }

  return false;
}

void PlotWidget::overrideCursonMove()
{
  QSettings settings;
  QString theme = settings.value("Preferences::theme", "style_light").toString();
  QPixmap pixmap(tr(":/%1/move.png").arg(theme));
  QApplication::setOverrideCursor(QCursor(pixmap.scaled(24, 24)));
}

bool PlotWidget::canvasEventFilter(QEvent* event)
{
  switch (event->type())
  {
    case QEvent::Wheel: {
      auto mouse_event = dynamic_cast<QWheelEvent*>(event);

      bool ctrl_modifier = mouse_event->modifiers() == Qt::ControlModifier;
      auto legend_rect = _legend->geometry(canvas()->rect());

      if (ctrl_modifier)
      {
        if (legend_rect.contains(mouse_event->pos()) && _legend->isVisible())
        {
          int prev_size = _legend->font().pointSize();
          int new_size = prev_size;
          if (mouse_event->delta() > 0)
          {
            new_size = std::min(13, prev_size+1);
          }
          if (mouse_event->delta() < 0)
          {
            new_size = std::max(7, prev_size-1);
          }
          if( new_size != prev_size)
          {
            setLegendSize(new_size);
            emit legendSizeChanged(new_size);
          }
          return true;
        }
      }
      return false;
    }

    case QEvent::MouseButtonPress: {
      if (_dragging.mode != DragInfo::NONE)
      {
        return true;  // don't pass to canvas().
      }

      QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

      if (mouse_event->button() == Qt::LeftButton)
      {
        const QPoint press_point = mouse_event->pos();
        if (mouse_event->modifiers() == Qt::ShiftModifier)  // time tracker
        {
          QPointF pointF(invTransform(xBottom, press_point.x()), invTransform(yLeft, press_point.y()));
          emit trackerMoved(pointF);
          return true;  // don't pass to canvas().
        }
        else if (mouse_event->modifiers() == Qt::ControlModifier)  // panner
        {
          overrideCursonMove();
        }
        else
        {
          auto clicked_item = _legend->processMousePressEvent(mouse_event);
          if (clicked_item)
          {
            for (const auto& curve_it : _curve_list)
            {
              if (clicked_item == curve_it.second)
              {
                auto& curve = _curve_list.at(curve_it.first);
                curve->setVisible(!curve->isVisible());
                _tracker->redraw();
                on_zoomOutVertical_triggered();
                replot();
                return true;
              }
            }
          }
        }
        return false;  // send to canvas()
      }
      else if (mouse_event->buttons() == Qt::MidButton && mouse_event->modifiers() == Qt::NoModifier)
      {
        overrideCursonMove();
        return false;
      }
      else if (mouse_event->button() == Qt::RightButton)
      {
        if (mouse_event->modifiers() == Qt::NoModifier)  // show menu
        {
          canvasContextMenuTriggered(mouse_event->pos());
          return true;  // don't pass to canvas().
        }
      }
    }
    break;
      //---------------------------------
    case QEvent::MouseMove: {
      if (_dragging.mode != DragInfo::NONE)
      {
        return true;  // don't pass to canvas().
      }

      QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

      if (mouse_event->buttons() == Qt::LeftButton && mouse_event->modifiers() == Qt::ShiftModifier)
      {
        const QPoint point = mouse_event->pos();
        QPointF pointF(invTransform(xBottom, point.x()), invTransform(yLeft, point.y()));
        emit trackerMoved(pointF);
        return true;
      }
    }
    break;

    case QEvent::Leave: {
      if (_dragging.mode == DragInfo::NONE)
      {
        return false;
      }
    }
    break;
    case QEvent::MouseButtonRelease: {
      if (_dragging.mode == DragInfo::NONE)
      {
        QApplication::restoreOverrideCursor();
        return false;
      }
    }
    break;

    case QEvent::Enter: {
      // If you think that this code doesn't make sense, you are right.
      // This is the workaround I have eventually found to avoid the problem with spurious
      // QEvent::DragLeave (I have never found the origin of the bug).
      if (_dragging.mode != DragInfo::NONE)
      {
        dropEvent(nullptr);
      }
      return true;
    }

    default: {
    }

  }  // end switch

  return false;
}

void PlotWidget::setDefaultRangeX()
{
  if (_mapped_data.numeric.size() > 0)
  {
    double min = std::numeric_limits<double>::max();
    double max = -std::numeric_limits<double>::max();
    for (auto& it : _mapped_data.numeric)
    {
      const PlotData& data = it.second;
      if (data.size() > 0)
      {
        double A = data.front().x;
        double B = data.back().x;
        min = std::min(A, min);
        max = std::max(B, max);
      }
    }
    setAxisScale(xBottom, min - _time_offset, max - _time_offset);
  }
}

DataSeriesBase* PlotWidget::createCurveXY(const PlotData* data_x, const PlotData* data_y)
{
  DataSeriesBase* output = nullptr;

  try
  {
    output = new PointSeriesXY(data_x, data_y);
  }
  catch (std::runtime_error& ex)
  {
    if (if_xy_plot_failed_show_dialog)
    {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Warnings");
      msgBox.setText(tr("The creation of the XY plot failed with the following message:\n %1").arg(ex.what()));
      msgBox.addButton("Continue", QMessageBox::AcceptRole);
      msgBox.exec();
    }
    throw std::runtime_error("Creation of XY plot failed");
  }

  output->setTimeOffset(_time_offset);
  return output;
}

DataSeriesBase* PlotWidget::createTimeSeries(const QString& transform_ID, const PlotData* data)
{
  DataSeriesBase* output = new TimeSeries(data);

  // TODO transform_ID ??

/*  auto custom_it = _snippets.find(transform_ID);
  if (custom_it != _snippets.end())
  {
    const auto& snippet = custom_it->second;
    output = new CustomTimeseries(data, snippet, _mapped_data);
  }*/

  output->setTimeOffset(_time_offset);
  output->updateCache();
  return output;
}

void PlotWidget::changeBackgroundColor(QColor color)
{
  if (canvasBackground().color() != color)
  {
    setCanvasBackground(color);
    replot();
  }
}

void PlotWidget::setLegendSize(int size)
{
  auto font = _legend->font();
  font.setPointSize(size);
  _legend->setFont(font);
  replot();
}

bool PlotWidget::isLegendVisible() const
{
  return _legend && _legend->isVisible();
}

void PlotWidget::setLegendAlignment(Qt::Alignment alignment)
{
  _legend->setAlignmentInCanvas(Qt::Alignment(Qt::AlignTop | alignment));
}

void PlotWidget::setZoomEnabled(bool enabled)
{
  _zoom_enabled = enabled;
  _zoomer->setEnabled(enabled);
  _magnifier->setEnabled(enabled);
  _panner1->setEnabled(enabled);
  _panner2->setEnabled(enabled);
}

bool PlotWidget::isZoomEnabled() const
{
  return _zoom_enabled;
}

void PlotWidget::replot()
{
  if (_zoomer)
  {
    _zoomer->setZoomBase(false);
  }

  static int replot_count = 0;
  QwtPlot::replot();
   qDebug() << replot_count++;
}
