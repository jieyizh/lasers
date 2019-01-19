#include "gpiolib_addr.h"
#include "gpiolib_reg.h"

#include <stdint.h>
#include <stdio.h>		//for the printf() function
#include <fcntl.h>
#include <unistd.h> 		//needed for sleep
#include <sys/ioctl.h> 		//needed for the ioctl function
#include <stdlib.h> 		//for atoi
#include <time.h> 		//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()
#include <string.h> //for strcmpi
#include <signal.h>

//#define MYTEST

#ifndef MYTEST
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#endif

#define PRINT_MSG(file, time, programName, str) \
	do{ \
			fprintf(logFile, "%s : %s : %s", time, programName, str); \
			fflush(logFile); \
	}while(0) // Copied from Lab4Sample

//HARDWARE DEPENDENT CODE BELOW
#ifndef MARMOSET_TESTING
#define LASER_1_GPIO 5
#define LASER_2_GPIO 4

//Defined as global variables so they can be accessed in signle handlers
int watchdog = -1;
FILE* logFile = NULL;
char* programName = NULL;
GPIO_Handle _gpio = NULL;

//This function should initialize the GPIO pins
GPIO_Handle initializeGPIO()
{
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if (gpio == NULL)
	{
		perror("Could not initialize GPIO");
		return NULL;
	}
	return gpio;
}

//This function should accept the diode number (1 or 2) and output
//a 0 if the laser beam is not reaching the diode, a 1 if the laser
//beam is reaching the diode or -1 if an error occurs.
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber)
{
	if (gpio == NULL)
	{
		return -1;
	}

	if (diodeNumber == 1)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if (level_reg & (1 << LASER_1_GPIO))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else if (diodeNumber == 2)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if (level_reg & (1 << LASER_2_GPIO))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return -1;
	}
}

#endif
//END OF HARDWARE DEPENDENT CODE

//This function will output the number of times each laser was broken
//and it will output how many objects have moved into and out of the room.

//laser1Count will be how many times laser 1 is broken (the left laser).
//laser2Count will be how many times laser 2 is broken (the right laser).
//numberIn will be the number  of objects that moved into the room.
//numberOut will be the number of objects that moved out of the room.
void outputMessage(int laser1Count, int laser2Count,
		int numberIn, int numberOut)
{
	printf("Laser 1 was broken %d times \n", laser1Count);
	printf("Laser 2 was broken %d times \n", laser2Count);
	printf("%d objects entered the room \n", numberIn);
	printf("%d objects exited the room \n", numberOut);
}

//#ifndef MARMOSET_TESTING

#define CHECK_INTERVAL 50000 //50 milliseconds
#define CHECK_COUNT 100

#define MAX_LOGFILE 50
#define DEFAULT_LOGFILE "/home/pi/lab4.log"
#define DEFAULT_TIMEOUT 15

#define IS_BLANK(ch) (ch == '\t' || ch == ' ')
#define IS_COMMENT(ch) (ch == '#')
#define IS_EOL(ch) (ch == '\n' || ch == 0)
#define IS_DIGIT(ch) (ch >= '0' && ch <= '9')

int readConfig(FILE* configFile, int* timeout, char* logFileName)
{
	static char keyTimeout[] = "WATCHDOG_TIMEOUT";
	static char keyLogFile[] = "LOGFILE";

	enum State
	{
		START,
		DONE,
		READ_TIMEOUT,
		READ_LOG,
		CHECK_IMPORTANT,
		CHECK_NOT,
	};

	enum State s = START;

	int i = 0;
	int j = 0;
	char buffer[255];

	// The value of the timeout variable is set to zero at the start
	*timeout = 0;
	logFileName[0] = 0;

	while (fgets(buffer, 255, configFile) != NULL)
	{
		i = 0;
		s = START;

		while (s != DONE)
		{
			switch (s)
			{
			case START:
				if (IS_COMMENT(buffer[i]))
				{
					s = DONE; //line starts with '#'
				}
				//skip blanks
				else if (IS_BLANK(buffer[i]))
				{
					s = CHECK_NOT;
					i++;
				}
				else if (IS_EOL(buffer[i]))
				{
					s = DONE;
				}
				else if (buffer[i] != '#')
				{
					s = CHECK_IMPORTANT;
				}
				break;

			case CHECK_NOT:
				if (IS_BLANK(buffer[i]))
				{
					s = CHECK_NOT;
					i++;
				}
				else if (IS_COMMENT(buffer[i]) || IS_EOL(buffer[i]))
				{
					s = DONE;
					i++;
				}
				else
				{
					s = CHECK_IMPORTANT;
				}
				break;

			case CHECK_IMPORTANT:
				if (strncmp(buffer + i, keyTimeout, sizeof(keyTimeout) - 1) == 0)
				{
					s = READ_TIMEOUT;
					i = i + sizeof(keyTimeout) - 1;
				}
				else if (logFileName[0] == 0 && strncmp(buffer + i, keyLogFile, sizeof(keyLogFile) - 1) == 0)
				{
					s = READ_LOG;
					i = i + sizeof(keyLogFile) - 1;
				}
				else if (IS_EOL(buffer[i]) || IS_COMMENT(buffer[i]))
				{
					s = DONE;
					i++;
				}
				else
				{
					i++;
				}
				//i++;
				break;

			case READ_TIMEOUT:
				if (IS_BLANK(buffer[i]) || buffer[i] == '=')
				{
					s = READ_TIMEOUT;
				}
				else if (IS_COMMENT(buffer[i]) || IS_EOL(buffer[i]))
				{
					s = DONE;
				}
				else if (IS_DIGIT(buffer[i]))
				{
					*timeout = (*timeout * 10) + (buffer[i] - '0');
					s = READ_TIMEOUT;
				}
				else //invalid characters
				{
					printf("Invalid character %c\n", buffer[i]);
					s = DONE;
				}
				i++;
				break;

			case READ_LOG:
				if (IS_BLANK(buffer[i]) || buffer[i] == '=')
				{
					s = READ_LOG;
				}
				else if (IS_COMMENT(buffer[i]) || IS_EOL(buffer[i]))
				{
					s = DONE;
				}
				else if (buffer[i] != ' ' && buffer[i] != '=')
				{
					logFileName[j] = buffer[i];
					j++;
					if (j > MAX_LOGFILE)
					{
						return -1;
					}
				}
				else
				{
					s = DONE;
				}
				i++;
				break;

			case DONE:
				break;

			default:
				break;
			}
		}
	}

	return 0;
}

void getTime(char* buffer) // Copied from Lab4Sample
{
//Create a timeval struct named tv
	struct timeval tv;

//Create a time_t variable named curtime
	time_t curtime;

//Get the current time and store it in the tv struct
	gettimeofday(&tv, NULL);

//Set curtime to be equal to the number of seconds in tv
	curtime = tv.tv_sec;

//This will set buffer to be equal to a string that in
//equivalent to the current date, in a month, day, year and
//the current time in 24 hour notation.
	strftime(buffer, 30, "%m-%d-%Y  %T.", localtime(&curtime));

}

#define KICK_SECOND 1 //Kick watchdog every 1 second

void kickWatchdog(FILE* logFile, const char* programName, int watchdog)
{
	static time_t lastKick = (time_t)-1;

	if (lastKick == (time_t)-1)
	{
		lastKick = time(NULL);
	}

	time_t now = time(NULL);
	if ((now - lastKick) > KICK_SECOND)
	{
		lastKick = now;
		//ioctl(watchdog, WDIOC_KEEPALIVE, 0);

		char currTime[30];
		getTime(currTime);
		//Log that the Watchdog was kicked
		PRINT_MSG(logFile, currTime, programName, "The Watchdog was kicked\n\n");
	}

}

void stopService()
{
	if (watchdog < 0)
	{
		return;
	}

	char currTime[30];
	getTime(currTime);

	//Writing a V to the watchdog file will disable to watchdog and prevent it from
	//resetting the system
	write(watchdog, "V", 1);
	PRINT_MSG(logFile, currTime, programName, "The Watchdog was disabled\n\n");

	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	//Log that the Watchdog was closed
	PRINT_MSG(logFile, currTime, programName, "The Watchdog was closed\n\n");

	//Free the gpio pins
	gpiolib_free_gpio(_gpio);
	//Log that the GPIO pins were freed
	PRINT_MSG(logFile, currTime, programName, "The GPIO pins have been freed\n\n");

	fclose(logFile);
	//Stop with exit code 0
	exit(0);
}

int main(const int argc, const char* const argv[])
{
//Create a string that contains the program name
	const char* argName = argv[0];

//These variables will be used to count how long the name of the program is
	int i = 0;
	int namelength = 0;

	while (argName[i] != 0)
	{
		namelength++;
		i++;
	}

	char nameBuffer[namelength];
	programName = nameBuffer;

	i = 0;

	while (argName[i] != 0)
	{
		programName[i] = argName[i];
		i++;
	}
	programName[i] = 0;

//Create a file pointer named configFile
	FILE* configFile;
//Set configFile to point to the lab4.cfg file. It is
//set to read the file.
	configFile = fopen("/home/pi/lab4.cfg", "r");

//Output a warning message if the file cannot be openned
	if (!configFile)
	{
		perror("The config file could not be opened");
		return -1;
	}

//Declare the variables that will be passed to the readConfig function
	int timeout;
	char logFileName[MAX_LOGFILE];

//Call the readConfig function to read from the config file
	int retConfig = readConfig(configFile, &timeout, logFileName);

	//printf("timeout = %d\n", timeout);
	//printf("logFileName = %s\n", logFileName);

//Close the configFile now that we have finished reading from it
	fclose(configFile);

//Create a new file pointer to point to the log file
	//FILE* logFile;
//Set it to point to the file from the config file and make it append to
//the file when it writes to it.

	if (retConfig == -1  || logFileName[0] == 0)
	{
		printf("Open default log file %s\n", DEFAULT_LOGFILE);
		logFile = fopen(DEFAULT_LOGFILE, "a");
	}
	else
	{
		printf("Open log file %s\n", logFileName);
		logFile = fopen(logFileName, "a");
	}

//Check that the file opens properly.
	if (!logFile)
	{
		perror("The log file could not be opened");
		return -1;
	}

//Create a char array that will be used to hold the time values
	char currTime[30];
	getTime(currTime);

	if (retConfig == -1)
	{
		PRINT_MSG(logFile, currTime, programName,
				"Log file name too long, default value applied.\n\n");
	}

	if (timeout < 1 || timeout > 15)
	{
		timeout = DEFAULT_TIMEOUT;
		PRINT_MSG(logFile, currTime, programName,
				"Timeout not within parameters, default value applied.\n\n");

	}

//Initialize the GPIO pins
	_gpio = initializeGPIO();
//Get the current time
	getTime(currTime);

	if (_gpio == NULL)
	{
		PRINT_MSG(logFile, currTime, programName,
				"Could not initialize GPIO\n\n");
		fclose(configFile);
		return -1;
	}
//Log that the GPIO pins have been initialized
	PRINT_MSG(logFile, currTime, programName,
			"The GPIO pins have been initialized\n\n");

//This variable will be used to access the /dev/watchdog file, similar to how
//the GPIO_Handle works
	//int watchdog;

//We use the open function here to open the /dev/watchdog file. If it does
//not open, then we output an error message. We do not use fopen() because we
//do not want to create a file if it doesn't exist
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0)
	{
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		PRINT_MSG(logFile, currTime, programName,
				"Error: Couldn't open watchdog device!\n\n");
		fclose(configFile);
		return -1;
	}
//Get the current time
	getTime(currTime);
//Log that the watchdog file has been opened
	PRINT_MSG(logFile, currTime, programName,
			"The Watchdog file has been opened\n\n");

#ifndef MYTEST

//This line uses the ioctl function to set the time limit of the watchdog
//timer to 15 seconds. The time limit can not be set higher that 15 seconds
//so please make a note of that when creating your own programs.
//If we try to set it to any value greater than 15, then it will reject that
//value and continue to use the previously set time limit
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);
#endif

//Get the current time
	getTime(currTime);
//Log that the Watchdog time limit has been set
	PRINT_MSG(logFile, currTime, programName,
			"The Watchdog time limit has been set\n\n");

#ifndef MYTEST
//The value of timeout will be changed to whatever the current time limit of the
//watchdog timer is
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);
#endif

//This print statement will confirm to us if the time limit has been properly
//changed. The \n will create a newline character similar to what endl does.
	printf("The watchdog timeout is %d seconds.\n\n", timeout);

	//Set up signal handlers so watchdog can be disabled/closed
	//Interrupt by Ctrl-C
	signal(SIGINT, stopService);
	//Service stop
	signal(SIGTERM, stopService);
	//Kill process
	signal(SIGKILL, stopService);

	enum State
	{
		START,
		LEFT_IN,
		LEFT_BOTH,
		LEFT_LEAVE,
		RIGHT_OUT,
		RIGHT_BOTH,
		RIGHT_LEAVE
	};
	enum State s = START;
//enum State prevState = START;
	int laser1Count = 0;
	int laser2Count = 0;
	int numberIn = 0;
	int numberOut = 0;

	struct timeval  tv;
	gettimeofday(&tv, NULL);
	float lastKick = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

	while (1)
	{
		/*#ifdef MYTEST
			int left = 0;
			int right = 0;
			//GetLaserStatesTest(gpio, &left, &right);*/
		//#else
			int left = laserDiodeStatus(_gpio, 1); // Set laser1 on the left
			int right = laserDiodeStatus(_gpio, 2); // Set laser2 on the right
		//#endif

		switch (s)
		{
		case START:

			// 0 means light not received, 1 means light recieved
			if (left == 0 && right == 1) // laser1 is blocked from left side
			{
				++laser1Count;
				// Get the current time
				getTime(currTime);
				// Log that laser 1 has been broken
				PRINT_MSG(logFile, currTime, programName,
						"Laser 1 has been broken\n\n");
				s = LEFT_IN;

				break;
			}
			else if (left == 1 && right == 0) // laser2 is blocked from right side
			{
				++laser2Count;
				// Get the current time
				getTime(currTime);
				// Log that laser 2 has been broken
				PRINT_MSG(logFile, currTime, programName,
						"Laser 2 has been broken\n\n");
				s = RIGHT_OUT;

				break;
			}

			break;

		case LEFT_IN:
			if (left == 0 && right == 0) // both laser1 and laser2 are blocked
			{
				s = LEFT_BOTH;
				++laser2Count;
				// Get the current time
				getTime(currTime);
				// Log that laser 2 has been broken
				PRINT_MSG(logFile, currTime, programName,
						"Laser 2 has been broken\n\n");
				break;
			}
			else if (left == 1 && right == 1)
			{
				s = START;
				break;
			}
			else if (left == 0 && right == 1) // laser1 is still being blocked
			{
				s = LEFT_IN;
				break;
			}

			break;
		case LEFT_BOTH:
			if (left == 1 && right == 0)
			{
				s = LEFT_LEAVE;
				break;
			}
			else if (left == 0 && right == 1)
			{
				s = LEFT_IN;
				break;
			}
			else if (left == 0 && right == 0)
			{
				s = LEFT_BOTH;
				break;
			}
			break;
		case LEFT_LEAVE:
			if (left == 0 && right == 0)
			{
				s = LEFT_BOTH;
				break;
			}
			else if (left == 1 && right == 1)
			{
				s = START;
				numberIn++;
				// Get the current time
				getTime(currTime);
				// Log that an object has entered the room
				PRINT_MSG(logFile, currTime, programName,
						"An object has entered the room\n\n");
				break;
			}
			break;

		case RIGHT_OUT:
			if (left == 0 && right == 0)
			{
				s = RIGHT_BOTH;
				++laser1Count;
				// Get the current time
				getTime(currTime);
				// Log that laser 1 has been broken
				PRINT_MSG(logFile, currTime, programName,
						"Laser 1 has been broken\n\n");
				break;
			}
			else if (left == 1 && right == 1) // both laser1 and laser2 are blocked
			{
				s = START;
				break;
			}
			else if (left == 1 && right == 0) // laser1 is still being blocked
			{
				s = RIGHT_OUT;
				break;
			}
			break;

		case RIGHT_BOTH:
			if (left == 0 && right == 1)
			{
				s = RIGHT_LEAVE;
				break;
			}
			else if (left == 1 && right == 0)
			{
				s = RIGHT_OUT;
				break;
			}
			break;

		case RIGHT_LEAVE:
			if (left == 0 && right == 0)
			{
				s = RIGHT_BOTH;
				break;
			}
			else if (left == 1 && right == 1)
			{
				s = START;
				numberOut++;
				// Get the current time
				getTime(currTime);
				// Log that an object has exited the room
				PRINT_MSG(logFile, currTime, programName,
						"An object has exited the room\n\n");
				break;
			}
			break;

		default:
			return -1;
			break;
		}
		//char currTime[30];
		gettimeofday(&tv, NULL);
		float now = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

		//Kick watchdog every timeout/2 seconds
		if ((now - lastKick) > (timeout * 1000 / 2))
		{
			lastKick = now;
			ioctl(watchdog, WDIOC_KEEPALIVE, 0);

			getTime(currTime);
			//Log that the Watchdog was kicked
			PRINT_MSG(logFile, currTime, programName, "The Watchdog was kicked\n\n");
		}
	}

	//Never reach here
	stopService();
	//Return to end the program
	return 0;

}
