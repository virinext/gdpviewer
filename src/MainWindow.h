#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QMainWindow>
#include <QVBoxLayout>

#include <gst/gstbuffer.h>
#include <gst/gstevent.h>
#include <gst/gstcaps.h>

class MainWindow: public QMainWindow
{
	Q_OBJECT
	public:
		MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
		~MainWindow();

	public slots:
		void slotOpen();
		void slotAbout();


	protected:
	    void saveCustomData();
	    void readCustomData();

	private:
		bool process(const QString &fileName);
		void onBuffer(const GstBuffer *, QString &);
		void onEvent(GstEvent *, QString &);
		void onCaps(const GstCaps *, QString &);
};


#endif
