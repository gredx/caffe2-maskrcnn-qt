#include "ui_maskrcnn.h"
#include "maskrcnn.h"
#include "caffe2.h"
using namespace  std;

MaskRcnn::MaskRcnn(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MaskRcnn)
{
	imageLabel = new QLabel();
	net_label = new QLabel();
	mask_rcnnLabel = new QLabel();
	op.set_random_seed(1701);
	ctx_ = caffe2::make_unique<caffe2::CPUContext>(op);

	ui->setupUi(this);
}

MaskRcnn::~MaskRcnn()
{
	delete ui;
	delete imageLabel;
	delete net_label;
	delete mask_rcnnLabel;
}

bool MaskRcnn::load_init_net(string init_netName)
{
	QString temp = init_netName.c_str();
	temp = temp.split(".").back();
	if (temp == "txt" || temp == "pbtxt")
		caffe2::ReadProtoFromTextFile(init_netName, &init_net);
	else
		caffe2::ReadProtoFromBinaryFile(init_netName, &init_net);
	return true;
}

bool MaskRcnn::load_predict_net(string predict_netName)
{
	QString temp = predict_netName.c_str();
	temp = temp.split(".").back();
	if (temp == "txt" || temp == "pbtxt")
		caffe2::ReadProtoFromTextFile(predict_netName, &predict_net);
	else
		caffe2::ReadProtoFromBinaryFile(predict_netName, &predict_net);
	generate_netGraph();
	//this_thread::sleep_for(2s);
	show_net("dot.svg");
	return true;
}

bool MaskRcnn::load_imagenet_classes(string imagenet_classesName)
{
	imagenet_classes.clear();
	fstream classesFile(imagenet_classesName, ios::in);
	string s;
	for (int i = 0; getline(classesFile, s); i++) {
		//cout << i << ":  " << s.substr(s.find(" ")+1) << endl;
		imagenet_classes.push_back(s.substr(s.find(" ") + 1));
	}
	classesFile.close();
	return true;
}
// 根据predict_net生成网络图
bool MaskRcnn::generate_netGraph()
{
	/*digraph predict_net{
		node[shape = "box"]
		data,conv1_w,conv1_b->conv1[label = "Conv"]
		conv1->conv1[label = "Relu"]
		{conv1}->{pool1}[label = "MaxPool"]
		{pool1,fire2_squeeze1x1_w,fire2_squeeze1x1_b}->{fire2_squeeze1x1}[label = "Conv"]
		{fire2_expand1x1}->{fire2_expand1x1}[label = "Relu"]
	}*/
	fstream out("dot.d", ios::out);
	out << "digraph predict_net{" << endl;
	out << "node[shape=box]" << endl;
	//out << predict_net.op_size() << endl;
	draw_net(out, predict_net);
	out << "}" << endl;
	out.close();
	system("dot -Tsvg dot.d -o dot.svg");
	//WinExec("dot -Tpng dot.d -o dot.png", SW_HIDE);
	return false;
}


void MaskRcnn::show_net(string netImagePath)
{
	QString netImage = QString(netImagePath.c_str());

	if (!netImage.isEmpty()) {
		QPixmap img;
		img.load(netImage);
		net_label->resize(img.width(), img.height());
		net_label->setPixmap(img);
		ui->scrollArea_2->setWidget(net_label);
	}
}

string MaskRcnn::draw_net(fstream & out, const caffe2::NetDef net)
{
	string first;
	for (int i = 0; i < net.op_size(); i++)
	{
		string line;
		caffe2::OperatorDef op = net.op(i);
		// input... -> output [label=type]

		for (int j = 0; j < op.input_size(); j++)
		{
			line.append(op.input(j));
			line.append(",");
		}
		if (op.input_size())line.pop_back();
		if (i == 0)first = line;
		if (!line.empty())line.append("->");
		for (int j = 0; j < op.output_size(); j++)
		{
			line.append(op.output(j));
			line.append(",");
		}
		/*if (op.output_size() == 0) {
			line.pop_back();
			line.pop_back();
		}*/
		if (op.output_size())line.pop_back();
		if (op.type() == "If") {
			for (auto arg : op.arg()) {
				if (arg.has_n()) {
					// 有子图
					line.append(draw_net(out, arg.n()));
				}
			}
		}
		string FORBADED_CHAR = "/[]()";
		for (char ch : FORBADED_CHAR) {
			replace(line.begin(), line.end(), ch, '_');

		}
		line.append("[label=");
		line.append(op.type());
		line.append("]");

		out << line << endl;

	}
	return first;
}

// 选择图片
void MaskRcnn::on_pushButton_3_clicked()
{
	shared_ptr<QFileDialog> fileDialog = make_shared<QFileDialog>(this);
	fileDialog->setWindowTitle("选择图片");
	fileDialog->setDirectory(".");
	fileDialog->setViewMode(QFileDialog::Detail);
	if (fileDialog->exec() == QFileDialog::Accepted)
	{
		imgName = fileDialog->selectedFiles()[0];
		//cout<<imgName.toStdString()<<endl;
		ui->label_4->setText(imgName.split(QString("/")).back());
	}

	if (!imgName.isEmpty()) {
		QPixmap img;
		img.load(imgName);
		imageLabel->resize(img.width(), img.height());
		imageLabel->setPixmap(img);
		ui->scrollArea->setWidget(imageLabel);
	}

}
// 选择init_net
void MaskRcnn::on_pushButton_clicked()
{
	shared_ptr<QFileDialog> fileDialog = make_shared<QFileDialog>(this);
	fileDialog->setWindowTitle("选择init_net");
	fileDialog->setDirectory(".");
	fileDialog->setViewMode(QFileDialog::Detail);
	if (fileDialog->exec() == QFileDialog::Accepted)
	{
		init_netName = fileDialog->selectedFiles()[0];
		ui->label->setText(init_netName.split(QString("/")).back());
	}
	if(!init_netName.isEmpty()){
		load_init_net(init_netName.toStdString());
	}

}
// 选择predict_net
void MaskRcnn::on_pushButton_2_clicked()
{
	shared_ptr<QFileDialog> fileDialog = make_shared<QFileDialog>(this);
	fileDialog->setWindowTitle("选择predict_net");
	fileDialog->setDirectory(".");
	fileDialog->setViewMode(QFileDialog::Detail);
	if (fileDialog->exec() == QFileDialog::Accepted)
	{
		predict_netName = fileDialog->selectedFiles()[0];
		ui->label_2->setText(predict_netName.split(QString("/")).back());
	}
	if (!predict_netName.isEmpty()) {
		bool flag = load_predict_net(predict_netName.toStdString());
	}

}
// 选择分类结果文件 imagenet_classes.txt
void MaskRcnn::on_pushButton_5_clicked()
{
	shared_ptr<QFileDialog> fileDialog = make_shared<QFileDialog>(this);
	fileDialog->setWindowTitle("选择imagenet_classes");
	fileDialog->setDirectory(".");
	fileDialog->setViewMode(QFileDialog::Detail);
	if (fileDialog->exec() == QFileDialog::Accepted)
	{
		imagenet_classesName = fileDialog->selectedFiles()[0];
		ui->label_6->setText(imagenet_classesName.split(QString("/")).back());
	}
	if (!imagenet_classesName.isEmpty()) {
		load_imagenet_classes(imagenet_classesName.toStdString());
	}
}
// 执行预测
void MaskRcnn::on_pushButton_4_clicked()
{
	if (imgName.isEmpty())
	{
		QMessageBox::information(this, "error", "no image as input");
		return;
	}
	if (init_netName.isEmpty()) {
		QMessageBox::information(this, "error", "no init_net");
		return;
	}
	if (predict_netName.isEmpty()) {
		QMessageBox::information(this, "error", "no predict_net");
		return;
	}
	// 没有分类文件




	// 创建预测器
	caffe2::Predictor predictor(init_net, predict_net);
	caffe2::TensorCPU input;
	//QMessageBox::information(this, "title", imgName.toStdString().c_str());
	string str = imgName.toStdString();
	cv::Mat bgr_img = cv::imread(imgName.toStdString());
	if (bgr_img.data == NULL) {
		QMessageBox::information(this, "error", "read image fail");
		return;
	}
	vector<float> inputPlannar(1 * 3 * bgr_img.rows*bgr_img.cols);
	const int height = bgr_img.rows, width = bgr_img.cols;
	const int size = height * width;
	// 转换图片格式为float
	for (auto i = 0; i < height; i++)
	{
		for (auto j = 0; j < width; j++)
		{
			inputPlannar[i*width + j + 0 * size] = (float)bgr_img.data[(i*width + j) * 3 + 0];
			inputPlannar[i*width + j + 1 * size] = (float)bgr_img.data[(i*width + j) * 3 + 1];
			inputPlannar[i*width + j + 2 * size] = (float)bgr_img.data[(i*width + j) * 3 + 2];
		}
	}


	input.Resize(1, 3, height, width);
	cout << bgr_img.rows << " " << bgr_img.cols << endl;
	input.ShareExternalPointer(inputPlannar.data());
	cout << input.DebugString() << endl;

	// 执行预测
	clock_t start = clock();
	caffe2::Predictor::TensorVector inputVec{ &input }, outputVec;
	predictor.run(inputVec, &outputVec);
	clock_t finish = clock();
	cout << "predictor done!" << endl;
	cout << "use time : " << 1.0*(finish - start) / CLOCKS_PER_SEC << "s" << endl;

	// 输出预测结果
	//TODO : 计算输出结果,显示在GUI界面里
	// 对输出进行处理
		// 第一维包含各个分类的置信度
		// 第三维对应的是各个分类的索引
		// 第二维输出目标所在矩阵的左上和右下坐标
		// 第四维输出 各类的掩码
	if (outputVec.size() < 4) {
		QMessageBox::information(this, "error", "output error ,check if input net wrong");
		return;
	}
	auto firstOut = outputVec[0];
	auto secondOut = outputVec[1];
	auto thirdOut = outputVec[2];
	auto output = outputVec[3];

	// 标记目标区域

		//namedWindow("111");
	int row = secondOut->dims().at(0);
	int col = secondOut->dims().at(1);
	for (int i = 0; i < row; i++) {
		if (firstOut->data<float>()[i] < Confidence_level)continue;		// 置信度小于0.7的忽略

		for (int j = 0; j < col; j++)
		{
			cout << secondOut->data<float>()[i*col + j] << " ";
		}cout << endl;
		cv::Rect rect = cv::Rect(
			secondOut->data<float>()[i*col + 0],
			secondOut->data<float>()[i*col + 1],
			secondOut->data<float>()[i*col + 2] - secondOut->data<float>()[i*col + 0],
			secondOut->data<float>()[i*col + 3] - secondOut->data<float>()[i*col + 1]
		);
		if (rect.height < 4 || rect.width < 4) continue;
		cv::rectangle(bgr_img, rect, cv::Scalar(0, 0, 255), 1);
		string msg = to_string((int)thirdOut->data<float>()[i]) + " " + to_string(firstOut->data<float>()[i]);
		cv::Point pos = cv::Point(rect.x, rect.y);
		putText(bgr_img, msg, pos, cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 11), 1);

	}

	// 在原图上 填充标记的区域

	vector<int>dim;
	for (auto x : output->dims()) {
		dim.push_back(x);
	}
	for (int i = 0; i < dim[0]; i++)
	{
		if (firstOut->data<float>()[i] < 0.7)continue;
		cv::Mat mat = cv::Mat::zeros(dim[2], dim[3], CV_32FC1);		// 存储mask
		// 求出 平均的mask
		for (int j = 1; j < dim[1]; j++) {
			for (int k = 0; k < dim[2]; k++) {
				for (int l = 0; l < dim[3]; l++) {
					float fval = output->data<float>()[i*dim[1] * dim[2] * dim[3] + j * dim[2] * dim[3] + k * dim[3] + l];
					mat.at<float>(k, l) += fval;
				}
			}
		}
		for (int i = 0; i < dim[2]; i++)
		{
			for (int j = 0; j < dim[3]; j++)
			{
				mat.at<float>(i, j) /= (dim[1] - 1);
			}
		}
		cv::cvtColor(bgr_img, bgr_img, cv::COLOR_BGR2BGRA);
		cv::Mat larg;	// 插值恢复到roi区域大小
		int st = secondOut->data<float>()[i*col + 0];
		int en = secondOut->data<float>()[i*col + 1];
		int wid = secondOut->data<float>()[i*col + 2] - secondOut->data<float>()[i*col + 0];
		int heig = secondOut->data<float>()[i*col + 3] - secondOut->data<float>()[i*col + 1];
		if (wid < 10 || heig < 10) continue;
		cv::resize(mat, larg, cv::Size(wid, heig), 0, 0, cv::INTER_LINEAR);
		// 选择roi区域,进行填充
		cv::Mat roi = bgr_img(cv::Rect(st, en, wid, heig));

		for (int x = 0; x < roi.rows; x++) {
			for (int y = 0; y < roi.cols; y++) {
				float val = larg.at<float>(x, y);
				if (val > 0.5) {
					cv::Vec4b &roiD = roi.at<cv::Vec4b>(x, y);
					//cv::Vec4b plus = cv::Vec4b(0, 0, 0, mat.at<float>(i, j));
					roiD = roiD * 0.4 + cv::Vec4b((i + 25) * 267 % 255, (i + 34) * 107 % 255, (i + 10) * 53 % 255, 255)*0.6;
				}
			}
		}


	}
	
	
	// 显示处理后的原图
	cv::imwrite("mask-rcnn.jpg", bgr_img);
	
	QPixmap img("mask-rcnn.jpg");
	mask_rcnnLabel->resize(img.width(), img.height());
	mask_rcnnLabel->setPixmap(img);
	ui->scrollArea_3->setWidget(mask_rcnnLabel);
	
	

	// This is to allow us to use memory leak checks.
	google::protobuf::ShutdownProtobufLibrary();

}

void MaskRcnn::on_pushButton_6_clicked()
{
	Confidence_level = ui->lineEdit->text().toDouble();
	Confidence_level = max(0.0, Confidence_level);
}

