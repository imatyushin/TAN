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

	int returnCode(-1);

    try
    {
        returnCode = application.exec();
    }
	catch(const std::exception & exception)
	{
        std::cerr << exception.what() << std::endl;
    }

	return returnCode;
}
