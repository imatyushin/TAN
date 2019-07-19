#include "QTRoomAcousticConfig.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

	QApplication application(argc, argv);
	Q_INIT_RESOURCE(roomaccousticnew);

	application.setWindowIcon(QIcon(":/images/Resources/RoomAcousticsNew.png"));

	RoomAcousticQTConfig configWindow;
	configWindow.Init();

	return application.exec();
}
