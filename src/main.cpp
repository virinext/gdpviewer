#include <QApplication>

#include "MainWindow.h"
#include <gst/gst.h>
int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	gst_init(&argc, &argv);
	MainWindow wgt;
	wgt.show();

	return app.exec();
}