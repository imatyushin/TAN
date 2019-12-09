#include "RoomAcousticQT.h"
#include "GpuUtils.h"
#include "TrueAudioVR.h"
#include "FileUtility.h"

#include <time.h>
#include <ctime>
#include <cmath>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include "AclAPI.h"
#include <Shlwapi.h>
#include <cassert>
#else
#include <unistd.h>
#endif

#include <QStandardPaths>
#include <QSettings>

RoomAcousticQT::RoomAcousticQT()
{
	initialize();
}

RoomAcousticQT::~RoomAcousticQT()
{
}

void RoomAcousticQT::initialize()
{
	initializeEnvironment();
	initializeAudioEngine();

	initializeRoom();
	initializeConvolution();
	initializeListener();

	enumDevices();
}

bool RoomAcousticQT::start()
{
	// Remap the sound path to accomodates the audioVR engine
	// TODO: Need to port the configuration back to the audio3D engine

	//Initialize Audio 3D engine
	
	// Since cpu's device id is 0 in this demo, we need to decrease the device id if
	// you want to run GPU
	int convolutionDeviceIndex = mConvolutionOverCL ? mConvolutionDeviceIndex : 0;
	int roomDeviceIndex = mRoomOverCL ? mRoomDeviceIndex : 0;

	std::vector<std::string> fileNames;
	std::vector<bool> trackHead;

	fileNames.reserve(m_iNumOfWavFile);
	trackHead.reserve(m_iNumOfWavFile);

	for(int nameIndex(0); nameIndex < m_iNumOfWavFile; ++nameIndex)
	{
		if(mWavFileNames[nameIndex].length() && mSoundSourceEnable[nameIndex])
		{
			fileNames.push_back(mWavFileNames[nameIndex]);
			trackHead.push_back(mSrcTrackHead[nameIndex]);
		}
	}

	bool started = m_pAudioEngine->Init(
		mTANDLLPath,
		m_RoomDefinition,

		fileNames,

		mSrc1EnableMic,
		trackHead,

		m_iConvolutionLength,
		m_iBufferSize,

		mConvolutionOverCL,
		mConvolutionOverGPU,
		convolutionDeviceIndex,

#ifdef RTQ_ENABLED
		1 == mConvolutionPriority,
		2 == mConvolutionPriority,
		mConvolutionCUCount,
#endif

		mRoomOverCL,
		mRoomOverGPU,
		roomDeviceIndex,

#ifdef RTQ_ENABLED
		1 == mRoomPriority,
		2 == mRoomPriority,
		mRoomCUCount,
#endif

		m_eConvolutionMethod,

		mPlayerName
		);

	if(started)
	{
		m_pAudioEngine->setWorldToRoomCoordTransform(0., 0., 0., 0., 0., true);

		updateAllSoundSourcesPosition();
		updateListenerPosition();

		return m_pAudioEngine->Run();
	}

	return false;
}

void RoomAcousticQT::stop()
{
	m_pAudioEngine->Stop();
}

void RoomAcousticQT::initializeEnvironment()
{
	auto moduleFileName = getModuleFileName();
	auto path2Exe = getPath2File(moduleFileName);
	auto commandName = getFileNameWithoutExtension(moduleFileName);

	auto locations = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
	auto homeLocation = locations.size() ? locations[0].toStdString() : path2Exe;

	mTANDLLPath = path2Exe;
	mConfigFileName = joinPaths(homeLocation, std::string(".") + commandName + "-default.ini");
	mLogPath = joinPaths(homeLocation, std::string(".") + commandName + ".log");

	setCurrentDirectory(mTANDLLPath);

	//todo: ivm: uncomment
	/*
	FILE *fpLog = NULL;
	errno_t err = 0;
	errno = 0;

	// Redirect stdout and stderr to a log file.
	if (fopen_s(&fpLog, mLogPath.c_str(), "a+") == 0)
	{
		dup2(fileno(fpLog), fileno(stdout));
		dup2(fileno(fpLog), fileno(stderr));
	}
	//else if (fopen_s(&fpLog, "TaLibVRDemoLog.log", "a+") == 0) {
	//	dup2(fileno(fpLog), fileno(stdout));
	//	dup2(fileno(fpLog), fileno(stderr));
	//}
	*/

	//todo: ivm: return
	/*
	wchar_t logDateVerStamp[MAX_PATH * 2] = {0};
	char version[40] = {0};
	GetFileVersionAndDate(logDateVerStamp, version);
	wprintf(logDateVerStamp);
	*/
	std::cout << "Log started" << std::endl;

	for (int idx = 0; idx < MAX_SOURCES; idx++)
	{
		mWavFileNames[idx].resize(0);
	}

	for (int index = 0; index < MAX_SOURCES; ++index)
	{
		mSrcTrackHead[index] = false;
	}
}

void RoomAcousticQT::initializeAudioEngine()
{
	m_pAudioEngine = new Audio3D();
}

void RoomAcousticQT::initializeRoom()
{
	m_RoomDefinition.height = 4.0;
	m_RoomDefinition.width = 6.0;
	m_RoomDefinition.length = 10.0;

	m_RoomDefinition.mFront.damp = DBTODAMP(2.0);
	m_RoomDefinition.mBack.damp = DBTODAMP(6.0);

	m_RoomDefinition.mLeft.damp = m_RoomDefinition.mRight.damp = DBTODAMP(4.0);
	m_RoomDefinition.mTop.damp = m_RoomDefinition.mBottom.damp = DBTODAMP(2.0);
}

void RoomAcousticQT::initializeConvolution()
{
	m_iConvolutionLength = 32768;
	m_iBufferSize = 2048;
}

void RoomAcousticQT::initializeListener()
{
	m_Listener.earSpacing = float(0.16);
	m_Listener.headX = float(m_RoomDefinition.width * .8);
	m_Listener.headZ = float(m_RoomDefinition.length * .8);
	m_Listener.headY = 1.75;
	m_Listener.pitch = 0.0;
	m_Listener.roll = 0.0;
	m_Listener.yaw = 0.0;
}

void RoomAcousticQT::initializeAudioPosition(int index)
{
	assert(index >= 0 && index < MAX_SOURCES);

	float radius = m_RoomDefinition.width / 2;

	//for(int idx = 0; idx < MAX_SOURCES; idx++)
	{
		//original:
		/*m_SoundSources[idx].speakerX = m_RoomDefinition.width / 2 + idx * m_RoomDefinition.width / MAX_SOURCES;
		m_SoundSources[idx].speakerZ = float(m_RoomDefinition.width * 0.05);*/
		m_SoundSources[index].speakerX = m_Listener.headX + radius * std::sin(float(index));
		m_SoundSources[index].speakerZ = m_Listener.headZ + radius * std::cos(float(index));
		m_SoundSources[index].speakerY = 1.75;
	}
}

void RoomAcousticQT::enumDevices()
{
	{
		char buffer[MAX_DEVICES * MAX_PATH] = {0};
		char *devicesNames[MAX_DEVICES] = {0};

		for(int i = 0; i < MAX_DEVICES; i++)
		{
			devicesNames[i] = &buffer[i * MAX_PATH];
			devicesNames[i][0] = 0;
		}

		mCPUDevicesCount = listCpuDeviceNamesWrapper(devicesNames, MAX_DEVICES);

		for(int i = 0; i < MAX_DEVICES; i++)
		{
			mCPUDevicesNames[i] = devicesNames[i];
		}
	}

	{
		char buffer[MAX_DEVICES * MAX_PATH] = {0};
		char *devicesNames[MAX_DEVICES] = {0};

		for(int i = 0; i < MAX_DEVICES; i++)
		{
			devicesNames[i] = &buffer[i * MAX_PATH];
			devicesNames[i][0] = 0;
		}

		mGPUDevicesCount = listGpuDeviceNamesWrapper(devicesNames, MAX_DEVICES);

		for(int i = 0; i < MAX_DEVICES; i++)
		{
			mGPUDevicesNames[i] = devicesNames[i];
		}
	}
}

void RoomAcousticQT::loadConfiguration(const std::string& xmlfilename)
{
	QSettings settings(xmlfilename.c_str(), QSettings::IniFormat);
	settings.sync();

	auto created = settings.value("MAIN/Created").toString().toStdString();
	if(settings.status() != QSettings::NoError || !created.length())
	{
		//dont change anything for malformed files
		return;
	}
	
	m_iNumOfWavFile = settings.value("MAIN/Sources").toInt();

	for(int waveFileIndex(0); waveFileIndex < MAX_SOURCES; ++waveFileIndex)
	{
		if(waveFileIndex > m_iNumOfWavFile)
		{
			mWavFileNames[waveFileIndex].resize(0);
			mSoundSourceEnable[waveFileIndex] = false;
		}
		else
		{
			auto sourceName = std::string("SOURCES/Source") + std::to_string(waveFileIndex);

			mWavFileNames[waveFileIndex] = settings.value(sourceName.c_str()).toString().toStdString();
			mSoundSourceEnable[waveFileIndex] = settings.value((sourceName + "ON").c_str()).toInt() ? 1 : 0;
			if(!waveFileIndex)
			{
				mSrc1EnableMic = settings.value((sourceName + "MicEnable").c_str()).toInt() ? 1 : 0;
			}
			mSrcTrackHead[waveFileIndex] = settings.value((sourceName + "TracHeadPosition").c_str()).toInt() ? 1 : 0;

			m_SoundSources[waveFileIndex].speakerX = settings.value((sourceName + "SpeakerX").c_str()).toFloat();
			m_SoundSources[waveFileIndex].speakerY = settings.value((sourceName + "SpeakerY").c_str()).toFloat();
			m_SoundSources[waveFileIndex].speakerZ = settings.value((sourceName + "SpeakerZ").c_str()).toFloat();
		}
	}

	mPlayerName = settings.value("MAIN/Player").toString().toStdString();

	m_Listener.headX = settings.value((std::string("LISTENER/") + "HeadX").c_str()).toFloat();
	m_Listener.headY = settings.value((std::string("LISTENER/") + "HeadY").c_str()).toFloat();
	m_Listener.headZ = settings.value((std::string("LISTENER/") + "HeadZ").c_str()).toFloat();
	m_Listener.yaw = settings.value((std::string("LISTENER/") + "Yaw").c_str()).toFloat();
	m_Listener.pitch = settings.value((std::string("LISTENER/") + "Pitch").c_str()).toFloat();
	m_Listener.roll = settings.value((std::string("LISTENER/") + "Roll").c_str()).toFloat();

	m_RoomDefinition.width = settings.value((std::string("ROOM/") + "Width").c_str()).toFloat();
	m_RoomDefinition.height = settings.value((std::string("ROOM/") + "Height").c_str()).toFloat();
	m_RoomDefinition.length = settings.value((std::string("ROOM/") + "Length").c_str()).toFloat();
	m_RoomDefinition.mLeft.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampLeft").c_str()).toFloat());
	m_RoomDefinition.mRight.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampRight").c_str()).toFloat());
	m_RoomDefinition.mFront.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampFront").c_str()).toFloat());
	m_RoomDefinition.mBack.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampBack").c_str()).toFloat());
	m_RoomDefinition.mTop.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampTop").c_str()).toFloat());
	m_RoomDefinition.mBottom.damp = DBTODAMP(settings.value((std::string("ROOM/") + "DampBottom").c_str()).toFloat());

	mRoomOverCL = settings.value((std::string("ROOM/") + "OpenCL").c_str()).toInt() ? 1 : 0;
	mRoomOverGPU = settings.value((std::string("ROOM/") + "GPU").c_str()).toInt() ? 1 : 0;
	mRoomDeviceIndex = settings.value((std::string("ROOM/") + "Device").c_str()).toInt();
	mRoomPriority = settings.value((std::string("ROOM/") + "Priority").c_str()).toInt();
	mRoomCUCount = settings.value((std::string("ROOM/") + "CU").c_str()).toInt();

	//settings.value((std::string("CONVOLUTION/") + "Count").c_str());
	mConvolutionOverCL = settings.value((std::string("CONVOLUTION/") + "OpenCL").c_str()).toInt() ? 1 : 0;
	mConvolutionOverGPU = settings.value((std::string("CONVOLUTION/") + "GPU").c_str()).toInt() ? 1 : 0;
	mConvolutionDeviceIndex = settings.value((std::string("CONVOLUTION/") + "Device").c_str()).toInt();
	mConvolutionPriority = settings.value((std::string("CONVOLUTION/") + "Priority").c_str()).toInt();
	mConvolutionCUCount = settings.value((std::string("CONVOLUTION/") + "CU").c_str()).toInt();

	m_iConvolutionLength = settings.value((std::string("CONVOLUTION/") + "ConvolutionLength").c_str()).toInt();
	m_iBufferSize = settings.value((std::string("CONVOLUTION/") + "BufferSize").c_str()).toInt();
	m_eConvolutionMethod = (amf::TAN_CONVOLUTION_METHOD)settings.value((std::string("CONVOLUTION/") + "Method").c_str()).toInt();
}

/* Save Room acoustic configuration in xml file*/
void RoomAcousticQT::saveConfiguraiton(const std::string& xmlfilename)
{
	QSettings settings(xmlfilename.c_str(), QSettings::IniFormat);

	settings.setValue("MAIN/About", "This document was created by AMD RoomAcousticsDemo");
	settings.setValue("MAIN/Notes", "All dimensions in meters, damping in decibels");

	{
		time_t dt = {0};
		struct tm *lt = localtime(&dt);

		char buffer[MAX_PATH] = {0};
		std::snprintf(buffer,
			MAX_PATH,
			"%4d/%02d/%02d %02d:%02d:%02d",
			2000 + (lt->tm_year % 100), 
			1 + lt->tm_mon,
			lt->tm_mday, 
			lt->tm_hour, 
			lt->tm_min, 
			lt->tm_sec
			);

		settings.setValue(
			"MAIN/Created",
			buffer
			);
	}

	settings.setValue("MAIN/Sources", m_iNumOfWavFile);

	for(int waveFileIndex(0); waveFileIndex < m_iNumOfWavFile; ++waveFileIndex)
	{
		auto sourceName = std::string("SOURCES/Source") + std::to_string(waveFileIndex);

		settings.setValue(sourceName.c_str(), mWavFileNames[waveFileIndex].c_str());

		settings.setValue((sourceName + "ON").c_str(), mSoundSourceEnable[waveFileIndex] ? 1 : 0);
		if(!waveFileIndex)
		{
			settings.setValue((sourceName + "MicEnable").c_str(), mSrc1EnableMic ? 1 : 0);
		}
		settings.setValue((sourceName + "TracHeadPosition").c_str(), mSrcTrackHead[waveFileIndex] ? 1 : 0);

		settings.setValue((sourceName + "SpeakerX").c_str(), std::to_string(m_SoundSources[waveFileIndex].speakerX).c_str());
		settings.setValue((sourceName + "SpeakerY").c_str(), std::to_string(m_SoundSources[waveFileIndex].speakerY).c_str());
		settings.setValue((sourceName + "SpeakerZ").c_str(), std::to_string(m_SoundSources[waveFileIndex].speakerZ).c_str());
	}

	settings.setValue("MAIN/Player", mPlayerName.c_str());

	settings.setValue((std::string("LISTENER/") + "HeadX").c_str(), std::to_string(m_Listener.headX).c_str());
	settings.setValue((std::string("LISTENER/") + "HeadY").c_str(), std::to_string(m_Listener.headY).c_str());
	settings.setValue((std::string("LISTENER/") + "HeadZ").c_str(), std::to_string(m_Listener.headZ).c_str());
	settings.setValue((std::string("LISTENER/") + "Yaw").c_str(), std::to_string(m_Listener.yaw).c_str());
	settings.setValue((std::string("LISTENER/") + "Pitch").c_str(), std::to_string(m_Listener.pitch).c_str());
	settings.setValue((std::string("LISTENER/") + "Roll").c_str(), std::to_string(m_Listener.roll).c_str());

	settings.setValue((std::string("ROOM/") + "Width").c_str(), std::to_string(m_RoomDefinition.width).c_str());
	settings.setValue((std::string("ROOM/") + "Height").c_str(), std::to_string(m_RoomDefinition.height).c_str());
	settings.setValue((std::string("ROOM/") + "Length").c_str(), std::to_string(m_RoomDefinition.length).c_str());
	settings.setValue((std::string("ROOM/") + "DampLeft").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mLeft.damp)).c_str());
	settings.setValue((std::string("ROOM/") + "DampRight").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mRight.damp)).c_str());
	settings.setValue((std::string("ROOM/") + "DampFront").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mFront.damp)).c_str());
	settings.setValue((std::string("ROOM/") + "DampBack").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mBack.damp)).c_str());
	settings.setValue((std::string("ROOM/") + "DampTop").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mTop.damp)).c_str());
	settings.setValue((std::string("ROOM/") + "DampBottom").c_str(), std::to_string(DAMPTODB(m_RoomDefinition.mBottom.damp)).c_str());

	settings.setValue((std::string("ROOM/") + "OpenCL").c_str(), mRoomOverCL ? 1 : 0);
	settings.setValue((std::string("ROOM/") + "GPU").c_str(), mRoomOverGPU ? 1 : 0);
	settings.setValue((std::string("ROOM/") + "Device").c_str(), mRoomDeviceIndex);
	settings.setValue((std::string("ROOM/") + "Priority").c_str(), mRoomPriority);
	settings.setValue((std::string("ROOM/") + "CU").c_str(), mRoomCUCount);

	settings.setValue((std::string("CONVOLUTION/") + "OpenCL").c_str(), mConvolutionOverCL ? 1 : 0);
	settings.setValue((std::string("CONVOLUTION/") + "GPU").c_str(), mConvolutionOverGPU ? 1 : 0);
	settings.setValue((std::string("CONVOLUTION/") + "Device").c_str(), mConvolutionDeviceIndex);
	settings.setValue((std::string("CONVOLUTION/") + "Priority").c_str(), mConvolutionPriority);
	settings.setValue((std::string("CONVOLUTION/") + "CU").c_str(), mConvolutionCUCount);

	settings.setValue((std::string("CONVOLUTION/") + "ConvolutionLength").c_str(), m_iConvolutionLength);
	settings.setValue((std::string("CONVOLUTION/") + "BufferSize").c_str(), m_iBufferSize);
	settings.setValue((std::string("CONVOLUTION/") + "Method").c_str(), int(m_eConvolutionMethod));

	settings.sync();
}

int RoomAcousticQT::addSoundSource(const std::string& sourcename)
{
	// Find a empty sound source slots and assign to it.
	for (int i = 0; i < MAX_SOURCES; i++)
	{
		if(!mWavFileNames[i].length())
		{
			mWavFileNames[i] = sourcename;
			mSoundSourceEnable[i] = true;
			mSrcTrackHead[i] = false;
			if(!i)
			{
				mSrc1EnableMic = false;
			}

			initializeAudioPosition(i);

			++m_iNumOfWavFile;

			return i;
		}
	}

	// Reach the source limit, exiting
	return -1;
}

bool RoomAcousticQT::removeSoundSource(int index2Remove)
{
	// Check if the id is valid
	if(index2Remove >= 0 && index2Remove < m_iNumOfWavFile)
	{
		if(!index2Remove)
		{
			mSrc1EnableMic = false;
		}

		for(int index(index2Remove + 1); index < m_iNumOfWavFile; ++index)
		{
			mWavFileNames[index - 1] = mWavFileNames[index];
			mSoundSourceEnable[index - 1] = mSoundSourceEnable[index];
			m_SoundSources[index - 1] = m_SoundSources[index];
			mSrcTrackHead[index - 1] = mSrcTrackHead[index];
		}

		//reset last
		{
			mWavFileNames[m_iNumOfWavFile - 1].resize(0);
			mSoundSourceEnable[m_iNumOfWavFile - 1] = true;

			m_SoundSources[m_iNumOfWavFile - 1].speakerY = 0.0f;
			m_SoundSources[m_iNumOfWavFile - 1].speakerX = 0.0f;
			m_SoundSources[m_iNumOfWavFile - 1].speakerZ = 0.0f;
			
			mSrcTrackHead[m_iNumOfWavFile - 1] = false;
		}

		--m_iNumOfWavFile;
	}
	
	return false;
}

/*TODO: Need to rework this function for potential epislon comparison*/
bool RoomAcousticQT::isInsideRoom(float x, float y, float z)
{
	return (x >= 0.0f && x <= m_RoomDefinition.width) &&
		(y >= 0.0f && y <= m_RoomDefinition.length) &&
		(z >= 0.0f && z <= m_RoomDefinition.height);
}

int RoomAcousticQT::findSoundSource(const std::string& sourcename)
{
	for (int i = 0; i < MAX_SOURCES; i++)
	{
		if(!strcmp(sourcename.c_str(), mWavFileNames[i].c_str()))
		{
			return i;
		}
	}
	return -1;
}

float RoomAcousticQT::getReverbTime(float final_db, int* nreflections)
{
	return estimateReverbTime(this->m_RoomDefinition, final_db, nreflections);
}

float RoomAcousticQT::getConvolutionTime()
{
	return m_iConvolutionLength / 48000.0f;
}

float RoomAcousticQT::getBufferTime()
{
	return m_iBufferSize / 48000.0f;
}

std::vector<std::string> RoomAcousticQT::getCPUConvMethod() const
{
	return {
		"FFT OVERLAP ADD"
		};
}

std::vector<std::string> RoomAcousticQT::getGPUConvMethod() const
{
	return {
		"FFT OVERLAP ADD",
		"FFT UNIFORM PARTITIONED",
		"FHT UNIFORM PARTITIONED",
		"FHT UINFORM HEAD TAIL"
		};
}

amf::TAN_CONVOLUTION_METHOD RoomAcousticQT::getConvMethodFlag(const std::string& _name)
{
	if (_name == "FFT OVERLAP ADD")
		return TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;
	if (_name == "FFT UNIFORM PARTITIONED")
		return TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED;
	if (_name == "FHT UNIFORM PARTITIONED")
		return TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED;
	if (_name == "FHT UINFORM HEAD TAIL")
		return TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL;

	return TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;
}

void RoomAcousticQT::updateAllSoundSourcesPosition()
{
	for (int i = 0; i < MAX_SOURCES; i++)
	{
		if(mWavFileNames[i].length() && mSoundSourceEnable[i])
		{
			updateSoundSourcePosition(i);
		}
	}
}

void RoomAcousticQT::updateSoundSourcePosition(int index)
{
	m_pAudioEngine->updateSourcePosition(
		index,
		m_SoundSources[index].speakerX,
		m_SoundSources[index].speakerY,
		m_SoundSources[index].speakerZ
		);
}

void RoomAcousticQT::UpdateSoundSourcesPositions()
{
	return;
	for(int index = 0; index < MAX_SOURCES; index++)
	{
		if(mWavFileNames[index].length() && mSoundSourceEnable[index])
		{
			float deltaX = m_Listener.headX - m_SoundSources[index].speakerX;
			float deltaZ = m_Listener.headZ - m_SoundSources[index].speakerZ;

			float radius = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);

			m_SoundSources[index].speakerX += 0.5;
			m_SoundSources[index].speakerZ += 0.5;

			m_pAudioEngine->updateSourcePosition(
				index,
				m_SoundSources[index].speakerX,
				m_SoundSources[index].speakerY,
				m_SoundSources[index].speakerZ
				);
		}
	}
}

void RoomAcousticQT::updateListenerPosition()
{
	m_pAudioEngine->updateHeadPosition(m_Listener.headX, m_Listener.headY, m_Listener.headZ,
		m_Listener.yaw, m_Listener.pitch, m_Listener.roll);
}

void RoomAcousticQT::updateRoomDimention()
{
	m_pAudioEngine->updateRoomDimension(m_RoomDefinition.width, m_RoomDefinition.height, m_RoomDefinition.length);
}

void RoomAcousticQT::updateRoomDamping()
{
	m_pAudioEngine->updateRoomDamping(m_RoomDefinition.mLeft.damp, m_RoomDefinition.mRight.damp, m_RoomDefinition.mTop.damp,
		m_RoomDefinition.mBottom.damp, m_RoomDefinition.mFront.damp, m_RoomDefinition.mBack.damp);
}

AmdTrueAudioVR* RoomAcousticQT::getAMDTrueAudioVR()
{
	return m_pAudioEngine->getAMDTrueAudioVR();
}

TANConverterPtr RoomAcousticQT::getTANConverter()
{
	return m_pAudioEngine->getTANConverter();
}