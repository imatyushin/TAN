#include "ConsoleUtility.h"

bool initFromCommandLine(QApplication &application, QCommandLineParser &consoleOptions)
{
	bool result(false);

	consoleOptions.setApplicationDescription("RoomAqousticsQt Application.(c)");
	consoleOptions.addHelpOption();

	QCommandLineOption configFile("configFile", "Required option. Initialize config with <File>", "File");
	QCommandLineOption metricsFile("metricsFile", "Required option. Save all gathered metrics into <File>", "File");
	QCommandLineOption outputFile("outputFile", "Required option. Save RoomAcousticsQT output into <File>", "File");
	QCommandLineOption playDuration("playDuration", "Sets duration of simulation to <Time> in miliseconds", "Time");
	QCommandLineOption restartCounter("restartCounter", "Sets restart counter <RestartCount> - 1,2,...", "RestartCount");
	
	consoleOptions.addOption(configFile);
	consoleOptions.addOption(metricsFile);
	consoleOptions.addOption(outputFile);
	consoleOptions.addOption(playDuration);
	consoleOptions.addOption(restartCounter);
	consoleOptions.process(application);

	if (consoleOptions.isSet("configFile") && consoleOptions.isSet("metricsFile") && consoleOptions.isSet("outputFile"))
	{
		result = true;
		if (consoleOptions.isSet("restartCounter") && consoleOptions.isSet("playDuration"))
		{
			std::cerr << "Console mode warning: conflicting options restartCounter and playDuration!" << std::endl;
		}

		std::cout << "Console mode initiated:" << std::endl
				<< "Config file path: " << consoleOptions.value(configFile).toStdString() << std::endl
				<< "Output file path: " << consoleOptions.value(outputFile).toStdString() << std::endl
				<< "Play duration in miliseconds: " << consoleOptions.value(playDuration).toInt() << std::endl
				<< "Metrics file path: " << consoleOptions.value(metricsFile).toStdString() << std::endl
				<< "Restart count number: " << consoleOptions.value(restartCounter).toInt() << std::endl;
	}
	else
	{
		result = false;
		std::cerr << "Console mode error: Required option is missing!" << std::endl;
		consoleOptions.showHelp();
	}

	return result;
}
