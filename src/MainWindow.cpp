#include "MainWindow.h"
#include "dataprotocol.h"
#include "version_info.h"

#include <QToolBar>
#include <QAction>
#include <QIcon>
#include <QFileDialog>
#include <QMessageBox>
#include <QScopedArrayPointer>
#include <QProgressBar>
#include <QCoreApplication>
#include <QDebug>
#include <QScrollArea>
#include <QLabel>
#include <QSettings>
#include <QMenu>
#include <QMenuBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags):
	QMainWindow(parent, flags)
{
	gst_dp_init();

	QToolBar *ptb = addToolBar("Menu");

	QAction *pactOpen = ptb -> addAction("Open...");
	pactOpen -> setShortcut(QKeySequence("Ctrl+O"));
	connect(pactOpen, SIGNAL(triggered()), SLOT(slotOpen()));

	QMenu *pmenu = menuBar() -> addMenu("&File");
	pmenu -> addAction(pactOpen);
	addAction (pactOpen);

	pmenu -> addSeparator();

	pmenu -> addSeparator();
	pmenu -> addAction("Exit", this, SLOT(close()));

	pmenu = menuBar() -> addMenu("&Help");
	pmenu -> addAction ("About gdpviewer...", this, SLOT(slotAbout()));


	QWidget *pwgt = new QWidget();
	setCentralWidget(pwgt);

	readCustomData();
}


void MainWindow::closeEvent(QCloseEvent *pevent)
{
  saveCustomData();
  
  m_break = true;

  QWidget::closeEvent(pevent);
}

void MainWindow::saveCustomData()
{
  QSettings settings("virinext", "gdpviewer");
  settings.setValue("MainWindow/geometry", saveGeometry());
}


void MainWindow::readCustomData()
{
  QSettings settings("virinext", "gdpviewer");
  restoreGeometry(settings.value("MainWindow/geometry").toByteArray()); 
}


bool MainWindow::process(const QString &fileName)
{
	m_break = false;
	QFile file(fileName);


	if(!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(this, "File opening problem", "Problem with open file `" + fileName + "`for reading");
		return false;
	}

	QProgressBar *pprogressBar = new QProgressBar(NULL);
	pprogressBar -> setWindowTitle("Opening...");
	pprogressBar -> setMinimum(0);
	pprogressBar -> setMaximum(file.size() / 1024);
	pprogressBar -> setValue(0);

	Qt::WindowFlags flags = pprogressBar -> windowFlags();

	pprogressBar -> show();

	QTreeWidget *ptreeWidget= new QTreeWidget();

	ptreeWidget -> header() -> close();

	bool res = true;
	for(;file.bytesAvailable() >= GST_DP_HEADER_LENGTH;)
	{
		if(m_break)
		{
			res = false;
			break;
		}

		guint8 header[GST_DP_HEADER_LENGTH];
		qint64 readed = file.read((char *) header, GST_DP_HEADER_LENGTH);
		if(readed != GST_DP_HEADER_LENGTH || !gst_dp_validate_header (GST_DP_HEADER_LENGTH, header))
		{
			QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
			break;
		}

		qint64 payloadLength = gst_dp_header_payload_length(header);
		QScopedArrayPointer<guint8> payload;
		if(payloadLength > 0)
		{
			payload.reset(new guint8[payloadLength]);
			if(file.read((char *) payload.data(), payloadLength) != payloadLength)
			{
				QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
				break;
			}

		}

		QTreeWidgetItem *pitem = NULL;

		GstDPPayloadType payloadType = gst_dp_header_payload_type(header);
		if(payloadType == GST_DP_PAYLOAD_BUFFER)
		{
			GstBuffer *buff = gst_dp_buffer_from_header(GST_DP_HEADER_LENGTH, header);

			if(!buff)
			{
				QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
				break;				
			}

			if(payloadLength > 0)
			{
				if(!gst_dp_validate_payload(GST_DP_HEADER_LENGTH, header, payload.data()))
				{
					QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
					break;
				}

				GstMapInfo map;
				gst_buffer_map(buff, &map, GST_MAP_WRITE);

				memcpy(map.data, payload.data(), payloadLength);
				gst_buffer_unmap(buff, &map);
			}

			pitem = onBuffer(buff);
			gst_buffer_unref(buff);

		}
		else if(payloadType == GST_DP_PAYLOAD_CAPS)
		{
			GstCaps *caps = gst_dp_caps_from_packet(GST_DP_HEADER_LENGTH, header, payload.data());

			if(!caps)
			{
				QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
				break;				
			}

			pitem = onCaps(caps);
	        gst_caps_unref(caps);
		}
		else if(payloadType >= GST_DP_PAYLOAD_EVENT_NONE)
		{
			GstEvent *event = gst_dp_event_from_packet(GST_DP_HEADER_LENGTH, header, payload.data());

			pitem = onEvent(event);
			gst_event_unref(event);
		}
		else
		{
			QMessageBox::critical(this, "Incorrect file", "File `" + fileName + "` is incorrect gdp file");
			break;
		}

		std::size_t position = file.pos();
		pprogressBar -> setValue(position / 1024);
		QCoreApplication::processEvents();

		if(!pprogressBar -> isVisible())
		{
			return false;
		}

		if(pitem)
			ptreeWidget -> addTopLevelItem(pitem);
	}

	if(res)
		setCentralWidget(ptreeWidget);


	pprogressBar -> close();
	delete pprogressBar;

	return res;
}


void MainWindow::slotOpen()
{
	QString dir = QDir::currentPath();
	QSettings settings("virinext", "gdpviewer");

	if(settings.value("MainWindow/PrevDir").toString().length())
		dir = settings.value("MainWindow/PrevDir").toString();

	QString fileName = QFileDialog::getOpenFileName(this, "GDP File", dir);
	bool res = false;
	if(!fileName.isEmpty())
		res = process(fileName);

	if(res)
	{
		QFileInfo info(fileName);
		settings.setValue("MainWindow/PrevDir", info.absoluteDir().absolutePath());

		setWindowTitle(info.fileName());
	}
}


void MainWindow::slotAbout()
{
  QString message;
  message = "<center><b>gdpviewer</b></center>";
  message += "<center>virinext@gmail.com</center>";
  message += QString("<center>Version: ") + VERSION_STR + "</center>";
  message += "<center>GUI Based on Qt</center>";
  QMessageBox::about(this, "About", message);
}


QTreeWidgetItem *MainWindow::onBuffer(const GstBuffer *buff)
{
	QString timestamp = GST_BUFFER_PTS_IS_VALID(buff) ? QString::number(GST_BUFFER_PTS(buff)) : "not set";
	QString duration = GST_BUFFER_DURATION_IS_VALID(buff) ? QString::number(GST_BUFFER_DURATION(buff)) : "not set";
	QString offset = GST_BUFFER_OFFSET_IS_VALID(buff) ? QString::number(GST_BUFFER_OFFSET(buff)) : "not set";
	QString offset_end = GST_BUFFER_OFFSET_END_IS_VALID(buff) ? QString::number(GST_BUFFER_OFFSET_END(buff)) : "not set";
	QString size = QString::number(gst_buffer_get_size((GstBuffer *)buff));

	bool none = true;
	QString flags = "(";
	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_LIVE))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_LIVE";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_DECODE_ONLY))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_DECODE_ONLY";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_DISCONT))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_DISCONT";
		none = false;
	}
		if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_RESYNC))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_RESYNC";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_CORRUPTED))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_CORRUPTED";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_MARKER))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_MARKER";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_HEADER))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_HEADER";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_GAP))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_GAP";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_DROPPABLE))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_DROPPABLE";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_DELTA_UNIT))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_DELTA_UNIT";
		none = false;
	}

	if(GST_BUFFER_FLAG_IS_SET(buff, GST_BUFFER_FLAG_LAST))
	{
		if(!none)
			flags += ", ";
		flags += "GST_BUFFER_FLAG_LAST";
		none = false;
	}

	if(none)
		flags += "none)";
	else
		flags += ")";


	QTreeWidgetItem *pitem = new QTreeWidgetItem(QStringList("Buffer: pts = " + timestamp));

	pitem -> addChild(new QTreeWidgetItem(QStringList("timestamp = " + timestamp)));
	pitem -> addChild(new QTreeWidgetItem(QStringList("duration = " + duration)));
	pitem -> addChild(new QTreeWidgetItem(QStringList("size = " + size)));
	pitem -> addChild(new QTreeWidgetItem(QStringList("offset = " + offset)));
	pitem -> addChild(new QTreeWidgetItem(QStringList("offset_end = " + offset_end)));
	pitem -> addChild(new QTreeWidgetItem(QStringList("offset_end = " + offset_end)));

	return pitem;
}


QTreeWidgetItem *MainWindow::onEvent(GstEvent *event)
{
	QString timestamp = GST_EVENT_TIMESTAMP(event) != GST_CLOCK_TIME_NONE ? QString::number(GST_EVENT_TIMESTAMP(event)) : "not set";
	QString type = GST_EVENT_TYPE_NAME(event);

	QTreeWidgetItem *pitem = new QTreeWidgetItem(QStringList("Event: " + type));

	pitem -> addChild(new QTreeWidgetItem(QStringList("timestamp = " + timestamp)));


	if(GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP)
	{
		gboolean resetTime;
		gst_event_parse_flush_stop(event, &resetTime);

		pitem -> addChild(new QTreeWidgetItem(QStringList("reset_time = " + QString::number(resetTime))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_GAP)
	{
		GstClockTime timestamp, duration;
		gst_event_parse_gap(event, &timestamp, &duration);

		pitem -> addChild(new QTreeWidgetItem(QStringList("timestamp = " + QString::number(timestamp))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("duration = " + QString::number(duration))));

	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_STREAM_START)
	{
		const gchar *streamId;
		gst_event_parse_stream_start(event, &streamId);
		pitem -> addChild(new QTreeWidgetItem(QStringList("stream_id = " + QString(streamId))));

	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT)
	{
		const GstSegment *segment;
		gst_event_parse_segment(event, &segment);
		QString str = "flags = ";

		bool none = true;

		if(segment -> flags == GST_SEGMENT_FLAG_NONE)
			str += "GST_SEGMENT_FLAG_NONE";
		else
		{
			if(segment -> flags & GST_SEGMENT_FLAG_RESET)
			{
				str += "GST_SEGMENT_FLAG_RESET";
				none = false;
			}

			if(segment -> flags & GST_SEGMENT_FLAG_SKIP)
			{
				if(!none)
					str += " , ";
				str += "GST_SEGMENT_FLAG_SKIP";
				none = false;
			}

			if(segment -> flags & GST_SEGMENT_FLAG_SEGMENT)
			{
				if(!none)
					str += " , ";
				str += "GST_SEGMENT_FLAG_SEGMENT";
				none = false;
			}
		}

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("rate = " + QString::number(segment -> rate))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("applied_rate = " + QString::number(segment -> applied_rate))));


		str = "format = ";

		if(segment -> format == GST_FORMAT_UNDEFINED)
			str += "GST_FORMAT_UNDEFINED";
		else if(segment -> format == GST_FORMAT_DEFAULT)
			str += "GST_FORMAT_DEFAULT";
		else if(segment -> format == GST_FORMAT_BYTES)
			str += "GST_FORMAT_BYTES";
		else if(segment -> format == GST_FORMAT_TIME)
			str += "GST_FORMAT_TIME";
		else if(segment -> format == GST_FORMAT_BUFFERS)
			str += "GST_FORMAT_BUFFERS";
		else if(segment -> format == GST_FORMAT_PERCENT)
			str += "GST_FORMAT_PERCENT";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("base = " + QString::number(segment -> base))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("offset = " + QString::number(segment -> offset))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("start = " + QString::number(segment -> start))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("stop = " + QString::number(segment -> stop))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("time = " + QString::number(segment -> time))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("position = " + QString::number(segment -> position))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("duration = " + QString::number(segment -> duration))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_TAG)
	{
		GstTagList *tags;
		gst_event_parse_tag(event, &tags);
		gchar *str = gst_tag_list_to_string(tags);
		pitem -> addChild(new QTreeWidgetItem(QStringList("tags = " + (QString)str)));
		g_free(str);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_BUFFERSIZE)
	{
		GstFormat format;
		gint64 minsize, maxsize;
		gboolean async;

		gst_event_parse_buffer_size(event, &format, &minsize, &maxsize, &async);
		QString str = "format = ";

		if(format == GST_FORMAT_UNDEFINED)
			str += QString("GST_FORMAT_UNDEFINED");
		else if(format == GST_FORMAT_DEFAULT)
			str += QString("GST_FORMAT_DEFAULT");
		else if(format == GST_FORMAT_BYTES)
			str += QString("GST_FORMAT_BYTES");
		else if(format == GST_FORMAT_TIME)
			str += QString("GST_FORMAT_TIME");
		else if(format == GST_FORMAT_BUFFERS)
			str += QString("GST_FORMAT_BUFFERS");
		else if(format == GST_FORMAT_PERCENT)
			str += QString("GST_FORMAT_PERCENT");
		
		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("minsize = " + QString::number(minsize))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("maxsize = " + QString::number(maxsize))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("async = " + (QString)(async ? "true" : "false"))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_QOS)
	{
		GstQOSType type;
		gdouble proportion;
		GstClockTimeDiff diff;
		GstClockTime timestamp;

		gst_event_parse_qos(event, &type, &proportion, &diff, &timestamp);

		QString str = "type = ";
		if(type == GST_QOS_TYPE_OVERFLOW)
			str += "GST_QOS_TYPE_OVERFLOW";
		else if(type == GST_QOS_TYPE_UNDERFLOW)
			str += "GST_QOS_TYPE_UNDERFLOW";
		else if(type == GST_QOS_TYPE_THROTTLE)
			str += "GST_QOS_TYPE_THROTTLE";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("proportion = " + QString::number(proportion))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("diff = " + QString::number(diff))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("timestamp = " + QString::number(timestamp))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SEEK)
	{
		gdouble rate;
		GstFormat format;
		GstSeekFlags flags;
		GstSeekType start_type;
		gint64 start, stop;
		GstSeekType stop_type;

		gst_event_parse_seek(event, &rate, &format, &flags, &start_type, &start, &stop_type, &stop);

		QString str = "format = ";
		if(format == GST_FORMAT_UNDEFINED)
			str += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			str += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			str += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			str += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			str += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			str += "GST_FORMAT_PERCENT";

		pitem -> addChild(new QTreeWidgetItem(QStringList("rate = " + QString::number(rate))));
		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));

		str = "flags = ";

		if(flags == GST_SEEK_FLAG_NONE)
			str += "GST_SEEK_FLAG_NONE";
		else
		{
			bool none = true;

			if(flags & GST_SEEK_FLAG_FLUSH)
			{
				str += "GST_SEEK_FLAG_FLUSH";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_ACCURATE)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_ACCURATE";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_KEY_UNIT)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_KEY_UNIT";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SEGMENT)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_SEGMENT";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SKIP)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_SKIP";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SNAP_BEFORE)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_SNAP_BEFORE";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SNAP_AFTER)
			{
				if(!none)
					str += ", ";
				str += "GST_SEEK_FLAG_SNAP_AFTER";
				none = false;
			}
		}

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));

		str = "start_type = ";
		if(start_type == GST_SEEK_TYPE_NONE)
			str += "GST_SEEK_TYPE_NONE";
		else if(start_type == GST_SEEK_TYPE_SET)
			str += "GST_SEEK_TYPE_SET";
		else if(start_type == GST_SEEK_TYPE_END)
			str += "GST_SEEK_TYPE_END";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("start = " + QString::number(start))));

		str = "stop_type = ";
		if(stop_type == GST_SEEK_TYPE_NONE)
			str += "GST_SEEK_TYPE_NONE";
		else if(stop_type == GST_SEEK_TYPE_SET)
			str += "GST_SEEK_TYPE_SET";
		else if(stop_type == GST_SEEK_TYPE_END)
			str += "GST_SEEK_TYPE_END";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));

		pitem -> addChild(new QTreeWidgetItem(QStringList("stop = " + QString::number(stop))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_LATENCY)
	{
		GstClockTime latency;
		gst_event_parse_latency(event, &latency);

		pitem -> addChild(new QTreeWidgetItem(QStringList("latency = " + QString::number(latency))));	
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_STEP)
	{
		GstFormat format;
		guint64 amount;
		gdouble rate;
		gboolean flush, intermediate;

		gst_event_parse_step(event, &format, &amount, &rate, &flush, &intermediate);

		QString str = "format = ";

		if(format == GST_FORMAT_UNDEFINED)
			str += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			str += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			str += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			str += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			str += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			str += "GST_FORMAT_PERCENT";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("amount = " + QString::number(amount))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("rate = " + QString::number(rate))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("flush = " + (QString)(flush ? "true" : "false"))));
		pitem -> addChild(new QTreeWidgetItem(QStringList("intermediate = " + (QString)(intermediate ? "true" : "false"))));
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SINK_MESSAGE)
	{
		GstMessage *msg;
		gst_event_parse_sink_message(event, &msg);
		pitem -> addChild(new QTreeWidgetItem(QStringList("message_type = " + (QString)GST_MESSAGE_TYPE_NAME(msg))));
		gst_message_unref(msg);

		
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_CAPS)
	{
		GstCaps *caps;
		gst_event_parse_caps(event, &caps);
		gchar *str = gst_caps_to_string(caps);;
		pitem -> addChild(new QTreeWidgetItem(QStringList("caps = " + (QString)str)));
		g_free(str);

	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_TOC_SELECT)
	{
		gchar *uid;
		gst_event_parse_toc_select(event, &uid);

		pitem -> addChild(new QTreeWidgetItem(QStringList("uid = " + (QString)uid)));
		g_free(uid);

	}	
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT_DONE)
	{
		GstFormat format;
		gint64 position;

		gst_event_parse_segment_done(event, &format, &position);
		
		QString str = "format = ";
		if(format == GST_FORMAT_UNDEFINED)
			str += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			str += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			str += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			str += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			str += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			str += "GST_FORMAT_PERCENT";

		pitem -> addChild(new QTreeWidgetItem(QStringList(str)));
		pitem -> addChild(new QTreeWidgetItem(QStringList("position = " + QString::number(position))));
	}	

	return pitem;
}

QTreeWidgetItem *MainWindow::onCaps(const GstCaps *caps)
{
	QTreeWidgetItem *pitem = new QTreeWidgetItem(QStringList("Caps"));

	gchar *str = gst_caps_to_string(caps);

	pitem -> addChild(new QTreeWidgetItem(QStringList(str)));

	g_free (str);

	return pitem;
}
