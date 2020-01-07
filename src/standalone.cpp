// Copyright 2017-2019 Paul Nettle
//
// This file is part of Gobbledegook.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file in the root of the source tree.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// >>
// >>>  INSIDE THIS FILE
// >>
//
// This is an example single-file stand-alone application that runs a Gobbledegook server.
//
// >>
// >>>  DISCUSSION
// >>
//
// Very little is required ("MUST") by a stand-alone application to instantiate a valid Gobbledegook server. There are also some
// things that are reocommended ("SHOULD").
//
// * A stand-alone application MUST:
//
//     * Start the server via a call to `ggkStart()`.
//
//         Once started the server will run on its own thread.
//
//         Two of the parameters to `ggkStart()` are delegates responsible for providing data accessors for the server, a
//         `GGKServerDataGetter` delegate and a 'GGKServerDataSetter' delegate. The getter method simply receives a string name (for
//         example, "battery/level") and returns a void pointer to that data (for example: `(void *)&batteryLevel`). The setter does
//         the same only in reverse.
//
//         While the server is running, you will likely need to update the data being served. This is done by calling
//         `ggkNofifyUpdatedCharacteristic()` or `ggkNofifyUpdatedDescriptor()` with the full path to the characteristic or delegate
//         whose data has been updated. This will trigger your server's `onUpdatedValue()` method, which can perform whatever
//         actions are needed such as sending out a change notification (or in BlueZ parlance, a "PropertiesChanged" signal.)
//
// * A stand-alone application SHOULD:
//
//     * Shutdown the server before termination
//
//         Triggering the server to begin shutting down is done via a call to `ggkTriggerShutdown()`. This is a non-blocking method
//         that begins the asynchronous shutdown process.
//
//         Before your application terminates, it should wait for the server to be completely stopped. This is done via a call to
//         `ggkWait()`. If the server has not yet reached the `EStopped` state when `ggkWait()` is called, it will block until the
//         server has done so.
//
//         To avoid the blocking behavior of `ggkWait()`, ensure that the server has stopped before calling it. This can be done
//         by ensuring `ggkGetServerRunState() == EStopped`. Even if the server has stopped, it is recommended to call `ggkWait()`
//         to ensure the server has cleaned up all threads and other internals.
//
//         If you want to keep things simple, there is a method `ggkShutdownAndWait()` which will trigger the shutdown and then
//         block until the server has stopped.
//
//     * Implement signal handling to provide a clean shut-down
//
//         This is done by calling `ggkTriggerShutdown()` from any signal received that can terminate your application. For an
//         example of this, search for all occurrences of the string "signalHandler" in the code below.
//
//     * Register a custom logging mechanism with the server
//
//         This is done by calling each of the log registeration methods:
//
//             `ggkLogRegisterDebug()`
//             `ggkLogRegisterInfo()`
//             `ggkLogRegisterStatus()`
//             `ggkLogRegisterWarn()`
//             `ggkLogRegisterError()`
//             `ggkLogRegisterFatal()`
//             `ggkLogRegisterAlways()`
//             `ggkLogRegisterTrace()`
//
//         Each registration method manages a different log level. For a full description of these levels, see the header comment
//         in Logger.cpp.
//
//         The code below includes a simple logging mechanism that logs to stdout and filters logs based on a few command-line
//         options to specify the level of verbosity.
//
// >>
// >>>  Building with GOBBLEDEGOOK
// >>
//
// The Gobbledegook distribution includes this file as part of the Gobbledegook files with everything compiling to a single, stand-
// alone binary. It is built this way because Gobbledegook is not intended to be a generic library. You will need to make your
// custom modifications to it. Don't worry, a lot of work went into Gobbledegook to make it almost trivial to customize
// (see Server.cpp).
//
// If it is important to you or your build process that Gobbledegook exist as a library, you are welcome to do so. Just configure
// your build process to build the Gobbledegook files (minus this file) as a library and link against that instead. All that is
// required by applications linking to a Gobbledegook library is to include `include/Gobbledegook.h`.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <signal.h>
#include <iostream>
#include <thread>
#include <sstream>

#include "../include/Gobbledegook.h"
#include "../include/GattInterface.h"
#include "../include/GattService.h"
#include "../include/GattCharacteristic.h"
#include "../include/GattProperty.h"
#include "../include/GattDescriptor.h"
#include "../include/GattUuid.h"
#include "../include/ServerUtils.h"

//
// Constants
//

// Maximum time to wait for any single async process to timeout during initialization
static const int kMaxAsyncInitTimeoutMS = 30 * 1000;

//
// Server data values
//

// The battery level ("battery/level") reported by the server (see Server.cpp)
static uint8_t serverDataBatteryLevel = 78;

// The text string ("text/string") used by our custom text string service (see Server.cpp)
static std::string serverDataTextString = "Hello, world!";

//
// Logging
//

enum LogLevel
{
	Debug,
	Verbose,
	Normal,
	ErrorsOnly
};

// Our log level - defaulted to 'Normal' but can be modified via command-line options
LogLevel logLevel = Normal;

// Our full set of logging methods (we just log to stdout)
//
// NOTE: Some methods will only log if the appropriate `logLevel` is set
void LogDebug(const char *pText) { if (logLevel <= Debug) { std::cout << "  DEBUG: " << pText << std::endl; } }
void LogInfo(const char *pText) { if (logLevel <= Verbose) { std::cout << "   INFO: " << pText << std::endl; } }
void LogStatus(const char *pText) { if (logLevel <= Normal) { std::cout << " STATUS: " << pText << std::endl; } }
void LogWarn(const char *pText) { std::cout << "WARNING: " << pText << std::endl; }
void LogError(const char *pText) { std::cout << "!!ERROR: " << pText << std::endl; }
void LogFatal(const char *pText) { std::cout << "**FATAL: " << pText << std::endl; }
void LogAlways(const char *pText) { std::cout << "..Log..: " << pText << std::endl; }
void LogTrace(const char *pText) { std::cout << "-Trace-: " << pText << std::endl; }

//
// Signal handling
//

// We setup a couple Unix signals to perform graceful shutdown in the case of SIGTERM or get an SIGING (CTRL-C)
void signalHandler(int signum)
{
	switch (signum)
	{
		case SIGINT:
			LogStatus("SIGINT recieved, shutting down");
			ggkTriggerShutdown();
			break;
		case SIGTERM:
			LogStatus("SIGTERM recieved, shutting down");
			ggkTriggerShutdown();
			break;
	}
}

//
// Server data management
//

void serverConfigurator(ggk::DBusObject &dBusObject)
{
	// Service: Device Information (0x180A)
	//
	// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.device_information.xml
    dBusObject.gattServiceBegin("device", "180A")

		// Characteristic: Manufacturer Name String (0x2A29)
		//
		// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.manufacturer_name_string.xml
		.gattCharacteristicBegin("mfgr_name", "2A29", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnValue(pInvocation, "Acme Inc.", true);
			})

		.gattCharacteristicEnd()

		// Characteristic: Model Number String (0x2A24)
		//
		// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.model_number_string.xml
		.gattCharacteristicBegin("model_num", "2A24", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnValue(pInvocation, "Marvin-PA", true);
			})

		.gattCharacteristicEnd()

	.gattServiceEnd()

	// Battery Service (0x180F)
	//
	// This is a fake battery service that conforms to org.bluetooth.service.battery_service. For details, see:
	//
	//     https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.battery_service.xml
	//
	// We also handle updates to the battery level from inside the server (see onUpdatedValue). There is an external method
	// (see main.cpp) that updates our battery level and posts an update using ggkPushUpdateQueue. Those updates are used
	// to notify us that our value has changed, which translates into a call to `onUpdatedValue` from the idleFunc (see
	// Init.cpp).
	.gattServiceBegin("battery", "180F")

		// Characteristic: Battery Level (0x2A19)
		//
		// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.battery_level.xml
		.gattCharacteristicBegin("level", "2A19", {"read", "notify"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				uint8_t batteryLevel = self.getDataValue<uint8_t>("battery/level", 0);
				self.methodReturnValue(pInvocation, batteryLevel, true);
			})

			// Handle updates to the battery level
			//
			// Here we use the onUpdatedValue to set a callback that isn't exposed to BlueZ, but rather allows us to manage
			// updates to our value. These updates may have come from our own server or some other source.
			//
			// We can handle updates in any way we wish, but the most common use is to send a change notification.
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				uint8_t batteryLevel = self.getDataValue<uint8_t>("battery/level", 0);
				self.sendChangeNotificationValue(pConnection, batteryLevel);
				return true;
			})

		.gattCharacteristicEnd()
	.gattServiceEnd()

	// Current Time Service (0x1805)
	//
	// This is a time service that conforms to org.bluetooth.service.current_time. For details, see:
	//
	//    https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.current_time.xml
	//
	// Like the battery service, this also makes use of events. This one updates the time every tick.
	//
	// This showcases the use of events (see the call to .onEvent() below) for periodic actions. In this case, the action
	// taken is to update time every tick. This probably isn't a good idea for a production service, but it has been quite
	// useful for testing to ensure we're connected and updating.
	.gattServiceBegin("time", "1805")

		// Characteristic: Current Time (0x2A2B)
		//
		// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.current_time.xml
		.gattCharacteristicBegin("current", "2A2B", {"read", "notify"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnVariant(pInvocation, ggk::ServerUtils::gvariantCurrentTime(), true);
			})

			// Update the time every tick of the periodic timer
			//
			// We'll send an change notification to any subscribed clients with the latest value
			.onEvent(1, nullptr, CHARACTERISTIC_EVENT_CALLBACK_LAMBDA
			{
				self.sendChangeNotificationVariant(pConnection, ggk::ServerUtils::gvariantCurrentTime());
			})

		.gattCharacteristicEnd()

		// Characteristic: Local Time Information (0x2A0F)
		//
		// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.local_time_information.xml
		.gattCharacteristicBegin("local", "2A0F", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				self.methodReturnVariant(pInvocation, ggk::ServerUtils::gvariantLocalTime(), true);
			})

		.gattCharacteristicEnd()
	.gattServiceEnd()

	// Custom read/write text string service (00000001-1E3C-FAD4-74E2-97A033F1BFAA)
	//
	// This service will return a text string value (default: 'Hello, world!'). If the text value is updated, it will notify
	// that the value has been updated and provide the new text from that point forward.
	.gattServiceBegin("text", "00000001-1E3C-FAD4-74E2-97A033F1BFAA")

		// Characteristic: String value (custom: 00000002-1E3C-FAD4-74E2-97A033F1BFAA)
		.gattCharacteristicBegin("string", "00000002-1E3C-FAD4-74E2-97A033F1BFAA", {"read", "write", "notify"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				const char *pTextString = self.getDataPointer<const char *>("text/string", "");
				self.methodReturnValue(pInvocation, pTextString, true);
			})

			// Standard characteristic "WriteValue" method call
			.onWriteValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Update the text string value
				GVariant *pAyBuffer = g_variant_get_child_value(pParameters, 0);
				self.setDataPointer("text/string", ggk::Utils::stringFromGVariantByteArray(pAyBuffer).c_str());

				// Since all of these methods (onReadValue, onWriteValue, onUpdateValue) are all part of the same
				// Characteristic interface (which just so happens to be the same interface passed into our self
				// parameter) we can that parameter to call our own onUpdatedValue method
				self.callOnUpdatedValue(pConnection, pUserData);

				// Note: Even though the WriteValue method returns void, it's important to return like this, so that a
				// dbus "method_return" is sent, otherwise the client gets an error (ATT error code 0x0e"unlikely").
				// Only "write-without-response" works without this
				self.methodReturnVariant(pInvocation, NULL);
			})

			// Here we use the onUpdatedValue to set a callback that isn't exposed to BlueZ, but rather allows us to manage
			// updates to our value. These updates may have come from our own server or some other source.
			//
			// We can handle updates in any way we wish, but the most common use is to send a change notification.
			.onUpdatedValue(CHARACTERISTIC_UPDATED_VALUE_CALLBACK_LAMBDA
			{
				const char *pTextString = self.getDataPointer<const char *>("text/string", "");
				self.sendChangeNotificationValue(pConnection, pTextString);
				return true;
			})

			// GATT Descriptor: Characteristic User Description (0x2901)
			//
			// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_user_description.xml
			.gattDescriptorBegin("description", "2901", {"read"})

				// Standard descriptor "ReadValue" method call
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "A mutable text string used for testing. Read and write to me, it tickles!";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()

		.gattCharacteristicEnd()
	.gattServiceEnd()

	// Custom ASCII time string service
	//
	// This service will simply return the result of asctime() of the current local time. It's a nice test service to provide
	// a new value each time it is read.

	// Service: ASCII Time (custom: 00000001-1E3D-FAD4-74E2-97A033F1BFEE)
	.gattServiceBegin("ascii_time", "00000001-1E3D-FAD4-74E2-97A033F1BFEE")

		// Characteristic: ASCII Time String (custom: 00000002-1E3D-FAD4-74E2-97A033F1BFEE)
		.gattCharacteristicBegin("string", "00000002-1E3D-FAD4-74E2-97A033F1BFEE", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				// Get our local time string using asctime()
				time_t timeVal = time(nullptr);
				struct tm *pTimeStruct = localtime(&timeVal);
				std::string timeString = ggk::Utils::trim(asctime(pTimeStruct));

				self.methodReturnValue(pInvocation, timeString, true);
			})

			// GATT Descriptor: Characteristic User Description (0x2901)
			//
			// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_user_description.xml
			.gattDescriptorBegin("description", "2901", {"read"})

				// Standard descriptor "ReadValue" method call
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Returns the local time (as reported by POSIX asctime()) each time it is read";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()

		.gattCharacteristicEnd()
	.gattServiceEnd()

	// Custom CPU information service (custom: 0000B001-1E3D-FAD4-74E2-97A033F1BFEE)
	//
	// This is a cheezy little service that reads the CPU info from /proc/cpuinfo and returns the count and model of the
	// CPU. It may not work on all platforms, but it does provide yet another example of how to do things.

	// Service: CPU Information (custom: 0000B001-1E3D-FAD4-74E2-97A033F1BFEE)
	.gattServiceBegin("cpu", "0000B001-1E3D-FAD4-74E2-97A033F1BFEE")

		// Characteristic: CPU Count (custom: 0000B002-1E3D-FAD4-74E2-97A033F1BFEE)
		.gattCharacteristicBegin("count", "0000B002-1E3D-FAD4-74E2-97A033F1BFEE", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				int16_t cpuCount = 0;
				ggk::ServerUtils::getCpuInfo(cpuCount);
				self.methodReturnValue(pInvocation, cpuCount, true);
			})

			// GATT Descriptor: Characteristic User Description (0x2901)
			//
			// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_user_description.xml
			.gattDescriptorBegin("description", "2901", {"read"})

				// Standard descriptor "ReadValue" method call
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "This might represent the number of CPUs in the system";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()

		.gattCharacteristicEnd()

		// Characteristic: CPU Model (custom: 0000B003-1E3D-FAD4-74E2-97A033F1BFEE)
		.gattCharacteristicBegin("model", "0000B003-1E3D-FAD4-74E2-97A033F1BFEE", {"read"})

			// Standard characteristic "ReadValue" method call
			.onReadValue(CHARACTERISTIC_METHOD_CALLBACK_LAMBDA
			{
				int16_t cpuCount = 0;
				self.methodReturnValue(pInvocation, ggk::ServerUtils::getCpuInfo(cpuCount), true);
			})

			// GATT Descriptor: Characteristic User Description (0x2901)
			//
			// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_user_description.xml
			.gattDescriptorBegin("description", "2901", {"read"})

				// Standard descriptor "ReadValue" method call
				.onReadValue(DESCRIPTOR_METHOD_CALLBACK_LAMBDA
				{
					const char *pDescription = "Possibly the model of the CPU in the system";
					self.methodReturnValue(pInvocation, pDescription, true);
				})

			.gattDescriptorEnd()

		.gattCharacteristicEnd()
	.gattServiceEnd(); // << -- NOTE THE SEMICOLON
}

// Called by the server when it wants to retrieve a named value
//
// This method conforms to `GGKServerDataGetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
const void *dataGetter(const char *pName)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data getter");
		return nullptr;
	}

	std::string strName = pName;

	if (strName == "battery/level")
	{
		return &serverDataBatteryLevel;
	}
	else if (strName == "text/string")
	{
		return serverDataTextString.c_str();
	}

	LogWarn((std::string("Unknown name for server data getter request: '") + pName + "'").c_str());
	return nullptr;
}

// Called by the server when it wants to update a named value
//
// This method conforms to `GGKServerDataSetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
int dataSetter(const char *pName, const void *pData)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data setter");
		return 0;
	}
	if (nullptr == pData)
	{
		LogError("NULL pData sent to server data setter");
		return 0;
	}

	std::string strName = pName;

	if (strName == "battery/level")
	{
		serverDataBatteryLevel = *static_cast<const uint8_t *>(pData);
		LogDebug((std::string("Server data: battery level set to ") + std::to_string(serverDataBatteryLevel)).c_str());
		return 1;
	}
	else if (strName == "text/string")
	{
		serverDataTextString = static_cast<const char *>(pData);
		LogDebug((std::string("Server data: text string set to '") + serverDataTextString + "'").c_str());
		return 1;
	}

	LogWarn((std::string("Unknown name for server data setter request: '") + pName + "'").c_str());

	return 0;
}

//
// Entry point
//

int main(int argc, char **ppArgv)
{
	// A basic command-line parser
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = ppArgv[i];
		if (arg == "-q")
		{
			logLevel = ErrorsOnly;
		}
		else if (arg == "-v")
		{
			logLevel = Verbose;
		}
		else if  (arg == "-d")
		{
			logLevel = Debug;
		}
		else
		{
			LogFatal((std::string("Unknown parameter: '") + arg + "'").c_str());
			LogFatal("");
			LogFatal("Usage: standalone [-q | -v | -d]");
			return -1;
		}
	}

	// Setup our signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	// Register our loggers
	ggkLogRegisterDebug(LogDebug);
	ggkLogRegisterInfo(LogInfo);
	ggkLogRegisterStatus(LogStatus);
	ggkLogRegisterWarn(LogWarn);
	ggkLogRegisterError(LogError);
	ggkLogRegisterFatal(LogFatal);
	ggkLogRegisterAlways(LogAlways);
	ggkLogRegisterTrace(LogTrace);

	// Start the server's ascync processing
	//
	// This starts the server on a thread and begins the initialization process
	//
	// !!!IMPORTANT!!!
	//
	//     This first parameter (the service name) must match tha name configured in the D-Bus permissions. See the Readme.md file
	//     for more information.
	//
	if (!ggkStart("gobbledegook", "Gobbledegook", "Gobbledegook", serverConfigurator, dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
	{
		return -1;
	}

	// Wait for the server to start the shutdown process
	//
	// While we wait, every 15 ticks, drop the battery level by one percent until we reach 0
	while (ggkGetServerRunState() < EStopping)
	{
		std::this_thread::sleep_for(std::chrono::seconds(15));

		serverDataBatteryLevel = std::max(serverDataBatteryLevel - 1, 0);
		ggkNofifyUpdatedCharacteristic("/com/gobbledegook/battery/level");
	}

	// Wait for the server to come to a complete stop (CTRL-C from the command line)
	if (!ggkWait())
	{
		return -1;
	}

	// Return the final server health status as a success (0) or error (-1)
  	return ggkGetServerHealth() == EOk ? 0 : 1;
}
