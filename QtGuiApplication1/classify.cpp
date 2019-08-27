#include "ui_classify.h"
#include "classify.h"
#include "caffe2.h"
using namespace  std;

Classify::Classify(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::Classify)
{
	imageLabel = new QLabel();
	net_label = new QLabel();
	op.set_random_seed(1701);
	ctx_ = caffe2::make_unique<caffe2::CPUContext>(op);

	ui->setupUi(this);
}

Classify::~Classify()
{
	delete ui;
	delete imageLabel;
	delete net_label;
}

bool Classify::load_init_net(string init_netName)
{
	QString temp = init_netName.c_str();
	temp = temp.split(".").back();
	if (temp == "txt" || temp == "pbtxt")
		caffe2::ReadProtoFromTextFile(init_netName, &init_net);
	else
	 caffe2::ReadProtoFromBinaryFile(init_netName, &init_net);
	return true;
}

bool Classify::load_predict_net(string predict_netName)
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

bool Classify::load_imagenet_classes(string imagenet_classesName)
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
bool Classify::generate_netGraph()
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


void Classify::show_net(string netImagePath)
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

string Classify::draw_net(fstream & out, const caffe2::NetDef net)
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
		if(i==0)first = line;
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
void Classify::on_pushButton_3_clicked()
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
void Classify::on_pushButton_clicked()
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
	if (!init_netName.isEmpty()) {
		load_init_net(init_netName.toStdString());
	}
	
}
// 选择predict_net
void Classify::on_pushButton_2_clicked()
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
void Classify::on_pushButton_5_clicked()
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
void Classify::on_pushButton_4_clicked()
{
	if (imgName.isEmpty())
	{
		QMessageBox::information(this, "error", "未选择输入图片");
		return;
	}
	if (init_netName.isEmpty()) {
		QMessageBox::information(this, "error", "未选择init_net");
		return;
	}
	if (predict_netName.isEmpty()) {
		QMessageBox::information(this, "error", "未选择predict_net");
		return;
	}
	if (imagenet_classesName.isEmpty()) {
		QMessageBox::information(this, "error", "未选择分类结果文件");
		return;
	}



	// 创建预测器
	caffe2::Predictor predictor(init_net, predict_net);
	caffe2::TensorCPU input;
	//QMessageBox::information(this, "title", imgName.toStdString().c_str());
	string str = imgName.toStdString();
	cv::Mat bgr_img = cv::imread(imgName.toStdString());
	if (bgr_img.data == NULL) {
		QMessageBox::information(this, "error", "读取图片失败");
		return;
	}
	int height = bgr_img.rows;
	int width = bgr_img.cols;
	
	// 输入图像大小
	const int predHeight = 227;
	const int predWidth = 227;
	const int crops = 1;      // crops等于1表示batch的数量为1
	const int channels = 3;   // 通道数为3，表示BGR，为1表示灰度图
	const int size = predHeight * predWidth;
	const float hscale = ((float)height) / predHeight; // 计算缩放比例
	const float wscale = ((float)width) / predWidth;
	const float scale = std::min(hscale, wscale);
	
	// 初始化网络的输入，因为可能要做batch操作，所以分配一段连续的存储空间
	std::vector<float> inputPlanar(crops * channels * predHeight * predWidth);
	std::cout << "before resizing, bgr_img.cols=" << bgr_img.cols << ", bgr_img.rows=" << bgr_img.rows << std::endl;
	// resize成想要的输入大小
	cv::Size dsize = cv::Size(bgr_img.cols / wscale, bgr_img.rows / hscale);
	cv::resize(bgr_img, bgr_img, dsize);
	
	std::cout << "after resizing, bgr_img.cols=" << bgr_img.cols << ", bgr_img.rows=" << bgr_img.rows << std::endl;
	// Scale down the input to a reasonable predictor size.
	// 这里是将图像复制到连续的存储空间内，用于网络的输入，因为是BGR三通道，所以有三个赋值
	// 注意imread读入的图像格式是unsigned char，如果你的网络输入要求是float的话，下面的操作就不对了。
	for (auto i = 0; i < predHeight; i++) {
		//printf("+\n");
		for (auto j = 0; j < predWidth; j++) {
			inputPlanar[i * predWidth + j + 0 * size] = (float)bgr_img.data[(i*predWidth + j) * 3 + 0];
			inputPlanar[i * predWidth + j + 1 * size] = (float)bgr_img.data[(i*predWidth + j) * 3 + 1];
			inputPlanar[i * predWidth + j + 2 * size] = (float)bgr_img.data[(i*predWidth + j) * 3 + 2];
		}
	}
	// input就是网络的输入，所以把之前准备好的数据赋值给input就可以了
	input.Resize(std::vector<int>({ crops, channels, predHeight, predWidth }));
	input.ShareExternalPointer(inputPlanar.data());

	caffe2::Predictor::TensorVector inputVec{ &input }, outputVec;
	predictor.run(inputVec, &outputVec);

	// 输出预测结果
	results.clear();
	for (auto output : outputVec)
	{
		for (auto i = 0; i < output->size(); i++)
		{
			float val = output->data<float>()[i];
			results.push_back(P(val, i));
		}
	}
	sort(results.begin(), results.end(), [](P& a, P& b) {
		return a.first > b.first;
	});
	QStringList lis;
	QStringListModel *model = new QStringListModel(this);
	double all=0;
	for (int i = 0; i < std::min(10, (int)results.size()); i++)
	{
		string s = std::to_string(i) + "\t:";
		s.append(imagenet_classes[results[i].second]);
		s.append("\t" + to_string(results[i].first));
		all += results[i].first;
		lis.push_back(QString(s.c_str()));
	}
	all = 1.0 - all;
	string s("其余概率和为:");
	s.append(std::to_string(all).c_str());
	lis.push_back(QString( s.c_str())) ;
	model->setStringList(lis);
	ui->listView->setModel(model);
	"1 : cat  0.345";
	// This is to allow us to use memory leak checks.
	google::protobuf::ShutdownProtobufLibrary();

}
