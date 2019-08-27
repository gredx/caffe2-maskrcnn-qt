#pragma once

#include <QWidget>
#include "ui_MaskRcnn.h"
#include "caffe2.h"
namespace Ui {
	class MaskRcnn;
}

class MaskRcnn : public QWidget
{
	Q_OBJECT
public:
	explicit MaskRcnn(QWidget *parent = nullptr);
	~MaskRcnn();

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

	void on_pushButton_6_clicked();

private:
	Ui::MaskRcnn *ui;
	QLabel *imageLabel, *net_label,*mask_rcnnLabel;
	QString init_netName, predict_netName, imgName, imagenet_classesName;
	vector<string> imagenet_classes;
	caffe2::NetDef init_net, predict_net;
	caffe2::DeviceOption op;
	unique_ptr<caffe2::CPUContext> ctx_;
	double Confidence_level = 0.7;


	
	//caffe2::Predictor predictor;
};
