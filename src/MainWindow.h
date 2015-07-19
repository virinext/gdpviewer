#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QMainWindow>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QCloseEvent>

#include <gst/gstbuffer.h>
#include <gst/gstevent.h>
#include <gst/gstcaps.h>

class MainWindow: public QMainWindow
{
	Q_OBJECT
	public:
		MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);

	public slots:
		void slotOpen();
		void slotAbout();


	protected:
	    void saveCustomData();
	    void readCustomData();
		virtual void closeEvent(QCloseEvent *);


	private:
		bool process(const QString &fileName);
		QTreeWidgetItem *onBuffer(const GstBuffer *);
		QTreeWidgetItem *onEvent(GstEvent *);
		QTreeWidgetItem *onCaps(const GstCaps *);

		bool m_break;
};


#endif
