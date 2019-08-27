#ifndef CLASSIFY_H
#define CLASSIFY_H

#include "caffe2.h"
using namespace  std;
namespace Ui {
	class Classify;
}


class Classify :public QWidget
{
	Q_OBJECT

public:
	explicit Classify(QWidget *parent = nullptr);
	~Classify();
	
	typedef pair<double, int>P;
	bool load_init_net(string init_netName);
	bool load_predict_net(string predict_netName);
	bool load_imagenet_classes(string imagenet_classesName);
private:
	bool generate_netGraph();
	void show_net(string netImagePath);
	string draw_net(fstream &out, const caffe2::NetDef net);
private slots:
	void on_pushButton_3_clicked();

	void on_pushButton_clicked();

	void on_pushButton_2_clicked();

	void on_pushButton_5_clicked();

	void on_pushButton_4_clicked();

private:
	Ui::Classify *ui;
	QLabel *imageLabel,*net_label;
	QString init_netName, predict_netName, imgName, imagenet_classesName;
	vector<string> imagenet_classes;
	caffe2::NetDef init_net, predict_net;
	caffe2::DeviceOption op;
	unique_ptr<caffe2::CPUContext> ctx_;
	vector<P> results;
	//caffe2::Predictor predictor;
};

#endif // CLASSIFY_H
