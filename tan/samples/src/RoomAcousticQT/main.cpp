#include "QTRoomAcousticConfig.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	unsigned char a[32];
	unsigned char b[32];
	register __m256 *out1, *in1;
	out1 = (__m256 *)a;
	in1 = (__m256 *)b;

	memcpy(out1, in1, 256/8);
	*out1 = *in1;

	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

	QApplication application(argc, argv);
	Q_INIT_RESOURCE(roomaccousticnew);

	application.setWindowIcon(QIcon(":/images/Resources/RoomAcousticsNew.png"));

	RoomAcousticQTConfig configWindow;
	configWindow.Init();

	return application.exec();
}
