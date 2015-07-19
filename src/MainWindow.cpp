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


MainWindow::~MainWindow()
{
	saveCustomData();
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
	QFile file(fileName);


	if(!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(this, "File opening problem", "Problem with open file `" + fileName + "`for reading");
		return false;
	}

	QString content;

	QProgressBar *pprogressBar = new QProgressBar(NULL);
	pprogressBar -> setWindowTitle("Opening...");
	pprogressBar -> setMinimum(0);
	pprogressBar -> setMaximum(file.size());
	pprogressBar -> setValue(0);

	Qt::WindowFlags flags = pprogressBar -> windowFlags();

	pprogressBar -> show();

	for(;file.bytesAvailable() >= GST_DP_HEADER_LENGTH;)
	{
		guint8 header[GST_DP_HEADER_LENGTH];
		qint64 readed = file.read((char *) header, GST_DP_HEADER_LENGTH);
		if(readed != GST_DP_HEADER_LENGTH || !gst_dp_validate_header (GST_DP_HEADER_LENGTH, header))
		{
			QMessageBox::critical(this, "Incorrect file", "1File `" + fileName + "` is incorrect gdp file");
			return false;
		}

		qint64 payloadLength = gst_dp_header_payload_length(header);
		QScopedArrayPointer<guint8> payload;
		if(payloadLength > 0)
		{
			payload.reset(new guint8[payloadLength]);
			if(file.read((char *) payload.data(), payloadLength) != payloadLength)
			{
				QMessageBox::critical(this, "Incorrect file", "2File `" + fileName + "` is incorrect gdp file");
				return false;
			}

		}

		GstDPPayloadType payloadType = gst_dp_header_payload_type(header);
		if(payloadType == GST_DP_PAYLOAD_BUFFER)
		{
			GstBuffer *buff = gst_dp_buffer_from_header(GST_DP_HEADER_LENGTH, header);

			if(!buff)
			{
				QMessageBox::critical(this, "Incorrect file", "3File `" + fileName + "` is incorrect gdp file");
				return false;				
			}

			if(payloadLength > 0)
			{
				if(!gst_dp_validate_payload(GST_DP_HEADER_LENGTH, header, payload.data()))
				{
					QMessageBox::critical(this, "Incorrect file", "4File `" + fileName + "` is incorrect gdp file");
					return false;
				}

				GstMapInfo map;
				gst_buffer_map(buff, &map, GST_MAP_WRITE);

				memcpy(map.data, payload.data(), payloadLength);
				gst_buffer_unmap(buff, &map);
			}

			onBuffer(buff, content);
			gst_buffer_unref(buff);

		}
		else if(payloadType == GST_DP_PAYLOAD_CAPS)
		{
			GstCaps *caps = gst_dp_caps_from_packet(GST_DP_HEADER_LENGTH, header, payload.data());

			if(!caps)
			{
				QMessageBox::critical(this, "Incorrect file", "5File `" + fileName + "` is incorrect gdp file");
				return false;				
			}

			onCaps(caps, content);
	        gst_caps_unref(caps);
		}
		else if(payloadType >= GST_DP_PAYLOAD_EVENT_NONE)
		{
			GstEvent *event = gst_dp_event_from_packet(GST_DP_HEADER_LENGTH, header, payload.data());

			onEvent(event, content);
			gst_event_unref(event);
		}
		else
		{
			QMessageBox::critical(this, "Incorrect file", "6File `" + fileName + "` is incorrect gdp file");
			return false;
		}

		std::size_t position = file.pos();
		pprogressBar -> setValue(position);
		QCoreApplication::processEvents();

		if(!pprogressBar -> isVisible())
		{
			return false;
		}
	}
	QLabel *plbl = new QLabel(content);
	QScrollArea *pscrl = new QScrollArea;
	pscrl -> setWidget(plbl);
	setCentralWidget(pscrl);


	pprogressBar -> close();
	delete pprogressBar;

	return true;
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


void MainWindow::onBuffer(const GstBuffer *buff, QString &content)
{
	content += "<b>Buffer</b><br>"
				"&nbsp;&nbsp;&nbsp;&nbsp;Timespamp: " + 
					QString(GST_BUFFER_PTS_IS_VALID(buff) ? QString::number(GST_BUFFER_PTS(buff)) : "not set") + 
				" | Duration: " + 
					QString(GST_BUFFER_DURATION_IS_VALID(buff) ? QString::number(GST_BUFFER_DURATION(buff)) : "not set") + 
				" | Offset: " + 
					QString(GST_BUFFER_OFFSET_IS_VALID(buff) ? QString::number(GST_BUFFER_OFFSET(buff)) : "not set") + 
				" | Offset_end: " + 
					QString(GST_BUFFER_OFFSET_END_IS_VALID(buff) ? QString::number(GST_BUFFER_OFFSET_END(buff)) : "not set") + 
				"<br>"
				"&nbsp;&nbsp;&nbsp;&nbsp;Flags: " + QString::number(GST_BUFFER_FLAGS(buff));

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

	content += " " + flags + "<hr/>";
}

void MainWindow::onEvent(GstEvent *event, QString &content)
{
	content += QString("<b>Event</b><br>") + 
				"&nbsp;&nbsp;&nbsp;&nbsp;Timespamp: " + 
					QString(GST_EVENT_TIMESTAMP(event) != GST_CLOCK_TIME_NONE ? QString::number(GST_EVENT_TIMESTAMP(event)) : "not set") + 
				" | Type: " + GST_EVENT_TYPE_NAME(event);
	content += "<br>&nbsp;&nbsp;&nbsp;&nbsp;";
	if(GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP)
	{
		gboolean resetTime;
		gst_event_parse_flush_stop(event, &resetTime);
		content += "reset_time = " + QString::number(resetTime);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_GAP)
	{
		GstClockTime timestamp, duration;
		gst_event_parse_gap(event, &timestamp, &duration);
		content += "timestamp = " + QString::number(timestamp) + ", duration = " + QString::number(duration);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_STREAM_START)
	{
		const gchar *streamId;
		gst_event_parse_stream_start(event, &streamId);
		content += QString("stream_id = ") + streamId;		
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT)
	{
		const GstSegment *segment;
		gst_event_parse_segment(event, &segment);
		content += "flags = ";

		bool none = true;

		if(segment -> flags == GST_SEGMENT_FLAG_NONE)
			content += "GST_SEGMENT_FLAG_NONE";
		else
		{
			if(segment -> flags & GST_SEGMENT_FLAG_RESET)
			{
				content += "GST_SEGMENT_FLAG_RESET";
				none = false;
			}

			if(segment -> flags & GST_SEGMENT_FLAG_SKIP)
			{
				if(!none)
					content += " , ";
				content += "GST_SEGMENT_FLAG_SKIP";
				none = false;
			}

			if(segment -> flags & GST_SEGMENT_FLAG_SEGMENT)
			{
				if(!none)
					content += " , ";
				content += "GST_SEGMENT_FLAG_SEGMENT";
				none = false;
			}
		}

		content += ", rate = " + QString::number(segment -> rate);
		content += ", applied_rate = " + QString::number(segment -> applied_rate);
		content += ", format = ";

		if(segment -> format == GST_FORMAT_UNDEFINED)
			content += "GST_FORMAT_UNDEFINED";
		else if(segment -> format == GST_FORMAT_DEFAULT)
			content += "GST_FORMAT_DEFAULT";
		else if(segment -> format == GST_FORMAT_BYTES)
			content += "GST_FORMAT_BYTES";
		else if(segment -> format == GST_FORMAT_TIME)
			content += "GST_FORMAT_TIME";
		else if(segment -> format == GST_FORMAT_BUFFERS)
			content += "GST_FORMAT_BUFFERS";
		else if(segment -> format == GST_FORMAT_PERCENT)
			content += "GST_FORMAT_PERCENT";

		content += ", base = " + QString::number(segment -> base);
		content += ", offset = " + QString::number(segment -> offset);
		content += ", start = " + QString::number(segment -> start);
		content += ", stop = " + QString::number(segment -> stop);
		content += ", time = " + QString::number(segment -> time);
		content += ", position = " + QString::number(segment -> position);
		content += ", duration = " + QString::number(segment -> duration);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_TAG)
	{
		GstTagList *tags;
		gst_event_parse_tag(event, &tags);
		gchar *str = gst_tag_list_to_string(tags);
		content += QString("tags = ") + str;
		g_free(str);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_BUFFERSIZE)
	{
		GstFormat format;
		gint64 minsize, maxsize;
		gboolean async;

		gst_event_parse_buffer_size(event, &format, &minsize, &maxsize, &async);
		content += "format = ";

		if(format == GST_FORMAT_UNDEFINED)
			content += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			content += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			content += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			content += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			content += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			content += "GST_FORMAT_PERCENT";
		
		content += ", minsize = " + QString::number(minsize);
		content += ", maxsize = " + QString::number(maxsize);
		content += QString(", async = ") + (async ? "true" : "false");
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_QOS)
	{
		GstQOSType type;
		gdouble proportion;
		GstClockTimeDiff diff;
		GstClockTime timestamp;

		gst_event_parse_qos(event, &type, &proportion, &diff, &timestamp);

		content += "type = ";
		if(type == GST_QOS_TYPE_OVERFLOW)
			content += "GST_QOS_TYPE_OVERFLOW";
		else if(type == GST_QOS_TYPE_UNDERFLOW)
			content += "GST_QOS_TYPE_UNDERFLOW";
		else if(type == GST_QOS_TYPE_THROTTLE)
			content += "GST_QOS_TYPE_THROTTLE";

		content == ", proportion = " + QString::number(proportion);
		content == ", diff = " + QString::number(diff);
		content == ", timestamp = " + QString::number(timestamp);
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

		content += "rate = " + QString::number(rate);

		content += ", format = ";
		if(format == GST_FORMAT_UNDEFINED)
			content += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			content += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			content += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			content += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			content += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			content += "GST_FORMAT_PERCENT";

		content += ", flags = ";

		if(flags == GST_SEEK_FLAG_NONE)
			content += "GST_SEEK_FLAG_NONE";
		else
		{
			bool none = true;

			if(flags & GST_SEEK_FLAG_FLUSH)
			{
				content += "GST_SEEK_FLAG_FLUSH";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_ACCURATE)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_ACCURATE";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_KEY_UNIT)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_KEY_UNIT";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SEGMENT)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_SEGMENT";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SKIP)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_SKIP";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SNAP_BEFORE)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_SNAP_BEFORE";
				none = false;
			}

			if(flags & GST_SEEK_FLAG_SNAP_AFTER)
			{
				if(!none)
					content += ", ";
				content += "GST_SEEK_FLAG_SNAP_AFTER";
				none = false;
			}
		}

		content += ", start_type = ";
		if(start_type == GST_SEEK_TYPE_NONE)
			content += "GST_SEEK_TYPE_NONE";
		else if(start_type == GST_SEEK_TYPE_SET)
			content += "GST_SEEK_TYPE_SET";
		else if(start_type == GST_SEEK_TYPE_END)
			content += "GST_SEEK_TYPE_END";

		content += ", start = " + QString::number(start);

		content += ", stop_type = ";
		if(stop_type == GST_SEEK_TYPE_NONE)
			content += "GST_SEEK_TYPE_NONE";
		else if(stop_type == GST_SEEK_TYPE_SET)
			content += "GST_SEEK_TYPE_SET";
		else if(stop_type == GST_SEEK_TYPE_END)
			content += "GST_SEEK_TYPE_END";

		content += ", stop = " + QString::number(stop);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_LATENCY)
	{
		GstClockTime latency;
		gst_event_parse_latency(event, &latency);
		content += " latency = " + QString::number(latency);		
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_STEP)
	{
		GstFormat format;
		guint64 amount;
		gdouble rate;
		gboolean flush, intermediate;

		gst_event_parse_step(event, &format, &amount, &rate, &flush, &intermediate);

		content += "format = ";

		if(format == GST_FORMAT_UNDEFINED)
			content += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			content += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			content += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			content += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			content += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			content += "GST_FORMAT_PERCENT";

		content += ", amount = " + QString::number(amount);

		content += ", rate = " + QString::number(rate);
		if(flush)
			content += ", flush = true";
		else
			content += ", flush = false";

		if(intermediate)
			content += ", intermediate = true";
		else
			content += ", intermediate = false";
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SINK_MESSAGE)
	{
		GstMessage *msg;
		gst_event_parse_sink_message(event, &msg);
		content += QString("message_type = ") + GST_MESSAGE_TYPE_NAME(msg);
		gst_message_unref(msg);
		
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_CAPS)
	{
		GstCaps *caps;
		gst_event_parse_caps(event, &caps);
		gchar *str = gst_caps_to_string(caps);;
		content += QString("caps = ") + str;
		g_free(str);
	}
	else if(GST_EVENT_TYPE(event) == GST_EVENT_TOC_SELECT)
	{
		gchar *uid;
		gst_event_parse_toc_select(event, &uid);
		content += QString("uid = ") + uid;

		g_free(uid);
	}	
	else if(GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT_DONE)
	{
		GstFormat format;
		gint64 position;

		gst_event_parse_segment_done(event, &format, &position);
		
		content += "format = ";
		if(format == GST_FORMAT_UNDEFINED)
			content += "GST_FORMAT_UNDEFINED";
		else if(format == GST_FORMAT_DEFAULT)
			content += "GST_FORMAT_DEFAULT";
		else if(format == GST_FORMAT_BYTES)
			content += "GST_FORMAT_BYTES";
		else if(format == GST_FORMAT_TIME)
			content += "GST_FORMAT_TIME";
		else if(format == GST_FORMAT_BUFFERS)
			content += "GST_FORMAT_BUFFERS";
		else if(format == GST_FORMAT_PERCENT)
			content += "GST_FORMAT_PERCENT";

		content += ", position = " + QString::number(position);
	}	

	content += "<hr/>";
}

void MainWindow::onCaps(const GstCaps *caps, QString &content)
{
	gchar *str = gst_caps_to_string(caps);
	content += QString("<b>Caps</b><br>") + 
				"&nbsp;&nbsp;&nbsp;&nbsp;" + 
				str + 
				"<hr/>";
	g_free (str);
}
