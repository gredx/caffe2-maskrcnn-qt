#### 可视化使用caffe2 运行squeezenet 和 mask-rcnn 进行分类和 目标检测,分割

##### 开发环境
- QT 做界面
- 在VS中下载QT插件,进行QT 开发
- caffe2
- opencv
- eigen
- openblas
- protobuf dll
- graphviz windows安装版 (用于画网络图,可执行文件dot.exe必须放在环境变量里)
- squeezenet的两个参数文件,一个分类结果文件
- mask-rcnn两个参数文件

- release 版的dll库都已编译完成,放在dlls目录下,debug版自行编译

- 想要QT程序能在别的机器上运行,cd 编译结果exe所在目录  执行windeployqt excetools.exe

- 图形界面的使用说明:
	- 分两个tab,第一个tab是squeezenet的界面,第二个是mask-rcnn的界面
	- 在开始预测前,选择好网络定义文件predict_net , 参数文件init_net ,需处理的图片,分类的结果文件
	- squeezenet必须选择分类结果文件
	- mask-rcnn没有分类结果文件
	- mask-rcnn的置信度默认为0.7,最低0.5 ,可不设
	
	- 选择网络定义文件之后,程序会load参数文件 根据网络的op生成一个dot语言写的文本文件, 调用graphviz库的dot.exe 绘制网络结构图

- 文件组织结构:
	- 每个界面都有一个.ui文件 用于编辑界面元素,一个.h文件定义界面类的数据成员和动作,一个.cpp文件实现函数
	- mainwindow.ui .h .cpp  主界面,只有一个tabWidget 用于存两个子Widget
	- classify.ui .h .cpp		squeezenet 分类的界面
	- Mask-rcnn.ui .h .cpp		mask-rcnn 目标检测和分割界面
	- caffe2.h 	包含了必要的头文件