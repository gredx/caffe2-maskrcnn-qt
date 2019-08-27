#ifndef NODEBUG
#pragma comment(lib,"libprotobuf.lib")
#pragma comment(lib,"libprotobuf-lite.lib")
#pragma comment(lib,"libprotoc.lib")
#pragma comment(lib,"openblas035.lib")
#pragma comment(lib,"opencv_world401.lib")
#else

#endif // !NODEBUG


#include "QtGuiApplication1.h"
#include "classify.h"
#include "mainwindow.h"
#include <QtWidgets/QApplication>

using namespace cv;
using namespace std;


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	//QtGuiApplication1 w;
	//w.show();
	MainWindow main;
	main.show();
	//Classify c;
	//c.show();

	return a.exec();

}
