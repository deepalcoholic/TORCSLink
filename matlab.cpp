/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef _WIN32
#include<winsock2.h>
#include <windows.h>
#endif

#define BASE_PORT 8000   //The port on which to listen for incoming data

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <tgf.h>
#include <track.h>
#include <car.h>
#include <raceman.h>
#include <robottools.h>
#include <robot.h>

#define NBBOTS 10

static const char* botname[NBBOTS] = {
	"matlab 1", "matlab 2", "matlab 3", "matlab 4", "matlab 5",
	"matlab 6", "matlab 7", "matlab 8", "matlab 9", "matlab 10"
};

static const char* botdesc[NBBOTS] = {
	"matlab 1", "matlab 2", "matlab 3", "matlab 4", "matlab 5",
	"matlab 6", "matlab 7", "matlab 8", "matlab 9", "matlab 10"
};

static void initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s);
static void newRace(int index, tCarElt* car, tSituation *s);
static void drive(int index, tCarElt* car, tSituation *s);
static void shutdown(int index);
static int InitFuncPt(int index, void *pt);
static void endRace(int index, tCarElt *car, tSituation *s);


SOCKET sock[NBBOTS];
WSADATA wsa;


// Module entry point.
extern "C" int matlab(tModInfo *modInfo)
{
     
    //Initialise winsock
    WSAStartup(MAKEWORD(2,2),&wsa);
	
	int i;
	
	// Clear all structures.
	memset(modInfo, 0, 10*sizeof(tModInfo));

	for (i = 0; i < NBBOTS; i++) {
		modInfo[i].name    = strdup(botname[i]);	// name of the module (short).
		modInfo[i].desc    = strdup(botdesc[i]);	// Description of the module (can be long).
		modInfo[i].fctInit = InitFuncPt;			// Init function.
		modInfo[i].gfId    = ROB_IDENT;				// Supported framework version.
		modInfo[i].index   = i;						// Indices from 0 to 9.
	}
	return 0;
}


// Module interface initialization.
static int InitFuncPt(int index, void *pt)
{
	tRobotItf *itf = (tRobotItf *)pt;

	// Create robot instance for index.
//	driver[index] = new Driver(index);
	itf->rbNewTrack = initTrack;	// Give the robot the track view called.
	itf->rbNewRace  = newRace;		// Start a new race.
	itf->rbDrive    = drive;		// Drive during race.
	itf->rbPitCmd   = NULL;		// Pit commands.
	itf->rbEndRace  = endRace;		// End of the current race.
	itf->rbShutdown = shutdown;		// Called before the module is unloaded.
	itf->index      = index;		// Index used if multiple interfaces.
	return 0;
}


// Called for every track change or new race.
static void initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s)
{
	// Don't load any particular car set up
	*carParmHandle = NULL;
}




// Start a new race.
static void newRace(int index, tCarElt* car, tSituation *sit)
{     
	struct sockaddr_in server;

    //Create a socket
	sock[index] = socket(AF_INET, SOCK_DGRAM, 0 );
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(BASE_PORT + index);
    
	int buf = 20;
	setsockopt(sock[index],SOL_SOCKET,SO_RCVBUF,(const char*)&buf,sizeof(int));

    //Bind
    bind(sock[index],(struct sockaddr *)&server, sizeof(server));
}


// Drive during race.
static void drive(int index, tCarElt* car, tSituation *s)
{
	struct sockaddr_in si_other;
	int slen = sizeof(si_other);
	pollfd fds;
	fds.fd = sock[index];
	fds.events = POLLIN;
	fds.revents = -1;

	if (WSAPoll(&fds,1,0)) {
		if (fds.revents | POLLIN) {
			char recvBuf[20];
			recvfrom(sock[index], recvBuf, 20, 0, (struct sockaddr *) &si_other, &slen);
			car->ctrl.accelCmd = *(float*)(recvBuf);
			car->ctrl.brakeCmd = *(float*)(recvBuf+4);
			car->ctrl.clutchCmd = *(float*)(recvBuf+8);
			car->ctrl.gear = *(int*)(recvBuf+12);
			car->ctrl.steer = *(float*)(recvBuf+16);

			float dist = RtGetDistFromStart(car);
			float speed = car->_speed_x;
			float angle = RtTrackSideTgAngleL(&(car->_trkPos)) - car->_yaw;
			angle = fmod(angle + 2*PI,2*PI);
			if (angle > PI) {
				angle = angle - 2*PI;
			}
			float latErr = car->_trkPos.toMiddle;
			float radius = car->_trkPos.seg->radius;
			float rpm = car->_enginerpm;

			char sendBuf[24];
			memcpy(sendBuf,&dist,4);
			memcpy(sendBuf+4,&speed,4);
			memcpy(sendBuf+8,&angle,4);
			memcpy(sendBuf+12,&latErr,4);
			memcpy(sendBuf+16,&radius,4);
			memcpy(sendBuf+20,&rpm,4);
		


			sendto(sock[index],sendBuf,24,0,(const sockaddr *)&si_other,slen);
		}
	}
}


// End of the current race.
static void endRace(int index, tCarElt *car, tSituation *s)
{
	closesocket(sock[index]);
}


// Called before the module is unloaded.
static void shutdown(int index)
{
	WSACleanup();
}

