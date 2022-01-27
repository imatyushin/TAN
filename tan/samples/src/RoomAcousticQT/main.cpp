#include "QTRoomAcousticConfig.h"
#include <QtWidgets/QApplication>
#include "ConsoleUtility.h"

int main(int argc, char *argv[])
{
    auto sharedMemory = amf_create_shared_memory(
#ifndef TAN_NO_OPENCL
        "/TAN-CL"
#else
  #ifdef ENABLE_METAL
		"/TAN-AMF-METAL"
  #else
		"/TAN-AMF-CL"
  #endif
#endif
		);
	amf_delete_shared_memory(sharedMemory);

	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

	foreach(const QString & path, QCoreApplication::libraryPaths())
	{
		std::cout << "[" << path.toStdString() << "]" << std::endl;
	}

	QApplication application(argc, argv);
	Q_INIT_RESOURCE(roomaccousticnew);

	application.setWindowIcon(QIcon(":/images/Resources/RoomAcousticsNew.png"));

	RoomAcousticQTConfig configWindow;

	if (application.arguments().count() >= 2)
	{
		initFromCommandLine(application, configWindow.consoleOptions);
		configWindow.Init(true);
	}
	else
	{
		configWindow.Init();
	}


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
