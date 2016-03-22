/*
 *  csensor.cpp
 *  qcn
 *
 *  Created by Carl Christensen on 08/11/2007.
 *  Copyright 2007 Stanford University.  All rights reserved.
 *
 * Implementation file for CSensor base classes
 * note it requires a reference to the sm shared memory datastructure (CQCNShMem)
 */

#include "csensor.h"
#include <math.h>
#include <cmath>
#include <complex>
//#include <except.h>

#include "mysql.h"
#include <pthread.h>

#include <fstream>
#include <iostream>
#include <iomanip>

using namespace std;

// device ID
// TODO:set from argv
//const int device_id;
const int device_id = 100;

const bool isAllTimeRecord = true;

// Program path
// TODO:define.hに移動
const char *base_dir = "/home/pi/csn_raspi";
const char *data_dir = "/earthquakes";
const char *file_extension = ".inp";

// local MySQL
const char *hostname = "localhost";
const char *username = "root";
const char *password = "";
const char *database = "csn";

// seismic.balog.jp MySQL
/*
const char *hostname = "";
const char *username = "";
const char *password = "";
const char *database = "";
*/

unsigned long portnumber = 3306;
char insert_q[500];

MYSQL *mysql;
MYSQL_RES *g_res = NULL;

extern double dtime();

extern CQCNShMem* sm;
bool g_bStop = false;

// config for recording trigger
const float LTA_second = 10.0f;
const float STA_second = 3.0f;

int LTA_array_numbers = (int)(LTA_second / DT); //DT is defined in define.h
int STA_array_numbers = (int)(STA_second / DT); //DT is defined in define.h
int LTA_STA_diff = LTA_array_numbers - STA_array_numbers;

const float limitTimes = 3.0f;

const int triggerLimit = 4;
int triggerCount = 0;

const double recordTime = 60.0f; //second
double startRecordTime;

bool isEarthQuake = false;

// cmd name
char system_cmd[255];

CSensor::CSensor()
  :
    m_iType(SENSOR_NOTFOUND),
    m_port(-1),
    m_bSingleSampleDT(false),
    m_strSensor("")
{
}
/*
CSensor::CSensor(int d_id, double x_off, double y_off, double z_off)
  :
    m_iType(SENSOR_NOTFOUND),
    m_port(-1),
    m_bSingleSampleDT(false),
    m_strSensor("")
{
    device_id = d_id;
    x_offset = x_off;
    y_offset = y_off;
    z_offset = z_off;
}
*/
CSensor::~CSensor()
{
    if (m_port>-1) closePort();
}

bool CSensor::getSingleSampleDT()
{
   return m_bSingleSampleDT;
}

void CSensor::setSingleSampleDT(const bool bSingle)
{
   m_bSingleSampleDT = bSingle;
}

const char* CSensor::getSensorStr()
{
   return m_strSensor.c_str();
}

void CSensor::setSensorStr(const char* strIn)
{
    if (strIn)
       m_strSensor.assign(strIn);
    else
       m_strSensor.assign("");
}

void CSensor::setType(e_sensor esType)
{
   m_iType = esType;
}

void CSensor::setPort(const int iPort)
{
   m_port = iPort;
}

int CSensor::getPort()
{
   return m_port;
}

void CSensor::closePort()
{
    fprintf(stdout, "Closing port...\n");
}

const e_sensor CSensor::getTypeEnum()
{
   return m_iType;
}

const char* CSensor::getTypeStr()
{
   switch (m_iType) {
     case SENSOR_MAC_PPC_TYPE1:
        return "PPC Mac Type 1";
        break;
     case SENSOR_MAC_PPC_TYPE2:
        return "PPC Mac Type 2";
        break;
     case SENSOR_MAC_PPC_TYPE3:
        return "PPC Mac Type 3";
        break;
     case SENSOR_MAC_INTEL:
        return "Intel Mac";
        break;
     case SENSOR_WIN_THINKPAD:
        return "Lenovo Thinkpad";
        break;
     case SENSOR_WIN_HP:
        return "HP Laptop";
        break;
     case SENSOR_USB:
        return "JoyWarrior 24F8 USB";
        break;
     default:
        return "Not Found";
   }
}

// this is the heart of qcn -- it gets called 50-500 times a second!
inline bool CSensor::mean_xyz(const bool bVerbose)
{
/* This subroutine finds the mean amplitude for x,y, & z of the sudden motion
 * sensor in a window dt from time t0.
 */

	static long lLastSample = 10L;  // store last sample size, start at 10 so doesn't give less sleep below, but will if lastSample<3
	float x1,y1,z1;
	double dTimeDiff=0.0f;
	bool result = false;

	// set up pointers to array offset for ease in functions below
	float *px2, *py2, *pz2;
	double *pt2;

	if (g_bStop || !sm) throw EXCEPTION_SHUTDOWN;   // see if we're shutting down, if so throw an exception which gets caught in the sensor_thread

	px2 = (float*) &(sm->x0[sm->lOffset]);
	py2 = (float*) &(sm->y0[sm->lOffset]);
	pz2 = (float*) &(sm->z0[sm->lOffset]);
	pt2 = (double*) &(sm->t0[sm->lOffset]);
	sm->lSampleSize = 0L;

	*px2 = *py2 = *pz2 = *pt2 = 0.0f;  // zero sample averages

	// this will get executed at least once, then the time is checked to see if we have enough time left for more samples
	do {
		if (sm->lSampleSize < SAMPLE_SIZE) {  // note we only get a sample if sample size < 10
			x1 = y1 = z1 = 0.0f;
			result = read_xyz(x1, y1, z1);
			*px2 += x1;
			*py2 += y1;
			*pz2 += z1;
			sm->lSampleSize++; // only increment if not a single sample sensor
		}  // done sample size stuff

		// dt is in seconds, want to slice it into 10 (SAMPLING_FREQUENCY), put into microseconds, so multiply by 100000
		// using usleep saves all the FP math every millisecond

		// sleep for dt seconds, this is where the CPU time gets used up, for dt/10 6% CPU, for dt/1000 30% CPU!
		// note the use of the "lLastSample" -- if we are getting low sample rates i.e. due to an overworked computer,
		// let's drop the sleep time dramatically and hope it can "catch up"
		// usleep((long) lLastSample < 3 ? DT_MICROSECOND_SAMPLE/100 : DT_MICROSECOND_SAMPLE);

		usleep(DT_MICROSECOND_SAMPLE); // usually 2000, which is 2 ms or .002 seconds, 10 times finer than the .02 sec / 50 Hz sample rate
		sm->t0active = dtime(); // use the function in the util library (was used to set t0)
		dTimeDiff = sm->t0check - sm->t0active;  // t0check should be bigger than t0active by dt, when t0check = t0active we're done
	}
	while (dTimeDiff > 0.0f);

	//fprintf(stdout, "Sensor sampling info:  t0check=%f  t0active=%f  diff=%f  timeadj=%d  sample_size=%ld, dt=%f\n",
	//   sm->t0check, sm->t0active, dTimeDiff, sm->iNumReset, sm->lSampleSize, sm->dt);
	//fprintf(stdout, "sensorout,%f,%f,%f,%d,%ld,%f\n",
	//   sm->t0check, sm->t0active, dTimeDiff, sm->iNumReset, sm->lSampleSize, sm->dt);
	//fflush(stdout);

	lLastSample = sm->lSampleSize;

	// store values i.e. divide by sample size
	*px2 /= (double) sm->lSampleSize;
	*py2 /= (double) sm->lSampleSize;
	*pz2 /= (double) sm->lSampleSize;
	*pt2 = sm->t0active; // save the time into the array, this is the real clock time

	if (bVerbose) {
		// Everytime INSERT to MySQL
        if(isAllTimeRecord){
    		sprintf(insert_q,
                        "INSERT INTO Event (device_id, t0check, t0active, x_acc, y_acc, z_acc, sample_size, offset) VALUES('%d', '%f', '%f', '%f', '%f', '%f', '%ld', '%ld')",
                          device_id, sm->t0check, *pt2, *px2, *py2, *pz2, sm->lSampleSize, sm->lOffset);
    		query(insert_q);
        }

        // To check isEarthQuake
		preserve_xyz.push_back(*new PreserveXYZ(px2, py2, pz2, pt2, &(sm->t0check), &(sm->lSampleSize), &(sm->lOffset)));

		if( preserve_xyz.size() > LTA_array_numbers ){
			preserve_xyz.erase(preserve_xyz.begin());
		}

		if(isEarthQuake) { // While a recordTime, couldn't check isEarthQuake
            // set triggered data
		    triggered_xyz.push_back(*new PreserveXYZ(px2, py2, pz2, pt2, &(sm->t0check), &(sm->lSampleSize), &(sm->lOffset)));

			if(isQuitRecording()) {
                // output triggered data to file
                outputEarthQuake();

				//printf("Recording quits at %f\n", sm->t0check);
				isEarthQuake = false;
			}else if( !(isAllTimeRecord) ){
        		sprintf(insert_q,
                            "INSERT INTO Event (device_id, t0check, t0active, x_acc, y_acc, z_acc, sample_size, offset) VALUES('%d', '%f', '%f', '%f', '%f', '%f', '%ld', '%ld')",
                              device_id, sm->t0check, *pt2, *px2, *py2, *pz2, sm->lSampleSize, sm->lOffset);
                              query(insert_q);
            }
		} else { // Only check isEarthQuake
            isEarthQuake = isStrikeEarthQuake();
		}
	}

	// if active time is falling behind the checked (wall clock) time -- set equal, may have gone to sleep & woken up etc
	sm->t0check += sm->dt;	// t0check is the "ideal" time i.e. start time + the dt interval

	sm->ullSampleTotal += sm->lSampleSize;
	sm->ullSampleCount++;

	sm->fRealDT += fabs(sm->t0active - sm->t0check);

	if (fabs(dTimeDiff) > TIME_ERROR_SECONDS) { // if our times are different by a second, that's a big lag, so let's reset t0check to t0active
		if (bVerbose) {
			fprintf(stdout, "Timing error encountered t0check=%f  t0active=%f  diff=%f  timeadj=%d  sample_size=%ld, dt=%f, resetting...\n",
			sm->t0check, sm->t0active, dTimeDiff, sm->iNumReset, sm->lSampleSize, sm->dt);
		}

		#ifndef _DEBUG
			return false;   // if we're not debugging, this is a serious run-time problem, so reset time & counters & try again
		#endif
	}

    free(insert_q);
    delete [] PreserveXYZ;

	return true;
}

bool CSensor::isQuitRecording() {
	if((sm->t0check - startRecordTime) >= recordTime) {
		return true;
	}
	else return false;
}

bool CSensor::isStrikeEarthQuake()
{
	float LTA_z = 0.0f, STA_z = 0.0f;
	double LTA_average = 0.0f, STA_average = 0.0f;
    double LTA_z_offset = 0.0f;

	if(preserve_xyz.size() == LTA_array_numbers)
	{
		for(int i = preserve_xyz.size()-1; i >= 0; i--){
			LTA_z += preserve_xyz[i].tmp_z; // origin
        }
		LTA_z_offset = LTA_z / (double)LTA_array_numbers;

        LTA_z = 0.0f;
		for(int i = preserve_xyz.size()-1; i >= 0; i--)
		{
			LTA_z += fabs(preserve_xyz[i].tmp_z - LTA_z_offset);

			if(i == LTA_STA_diff) STA_z = LTA_z;
		}

		LTA_average = LTA_z / (double)LTA_array_numbers;
		STA_average = STA_z / (double)STA_array_numbers;

		//debug
		//if(fabs(LTA_average - STA_average) > 0.002) {
			//fprintf(stdout, "%f %f %f %f %f\n\n", LTA_z, STA_z, LTA_average, STA_average, (LTA_average / STA_average));
		//}

        if( (LTA_average == 0.0f) || (STA_average == 0.0f) ) return false;

		if( fabs(LTA_average * limitTimes) < fabs(STA_average) ) {
			triggerCount++;

			//fprintf(stdout, "%f %f %f %f %f  - Trigger COUNT = %d\n\n", LTA_z, STA_z, LTA_average, STA_average, (LTA_average - STA_average), triggerCount);
			if( triggerCount == triggerLimit ){
				triggerCount = 0;
				startRecordTime = preserve_xyz.back().tmp_id_t;
				//printf("Recording starts at %f\n", startRecordTime);	//for logging

                // make treggerd data
                triggered_xyz.erase(triggered_xyz.begin());
                triggered_xyz = preserve_xyz;

                // Trigger event execute.
                sprintf(system_cmd, "nohup %s/tools/propagation.sh %d %f %f &", base_dir, device_id, startRecordTime, (fabs(STA_average)/fabs(LTA_average)) );
                //printf("cmd - %s\n",system_cmd);
                system(system_cmd);

				return true;
			}
			else return false;
		} else {
			triggerCount = 0;
			return false;
		}
	}
	else
		return false;
}

bool CSensor::outputEarthQuake(){

    char filename[128];
    sprintf(filename, "%s%s/%d_%f%s", base_dir, data_dir, device_id, startRecordTime, file_extension);

	std::ofstream ofs( filename );
    if (!ofs.is_open()) {
        return false;
    }

    cout.setf(ios_base::fixed,ios_base::floatfield);
    for(int i = 0; i < triggered_xyz.size(); i++){
        ofs << setprecision(20) <<
		triggered_xyz[i].tmp_t << "," <<
                triggered_xyz[i].tmp_x << "," <<
                triggered_xyz[i].tmp_y << "," <<
                triggered_xyz[i].tmp_z << std::endl;
    }
    ofs.close();

    free(filename);

    return true;
}

int CSensor::connectDatabase(){
  mysql = mysql_init(NULL);
  if (NULL == mysql){
    printf("error: %sn", mysql_error(mysql));
    return -1;
  }
  if (NULL == mysql_real_connect(mysql, hostname, username, password, database, portnumber, NULL, 0)){
  // error
  printf("error: %sn", mysql_error(mysql));
  return -1;
} else {
  // success
}
  return 1;
}
void CSensor::disconnectDatabase(){
  if(g_res){
    //freeResult(g_res);
    g_res = NULL;
  }
  mysql_close(mysql);
}

MYSQL_RES *CSensor::query(char *sql_string){
  if(g_res){
    freeResult(g_res);
    g_res = NULL;
  }

  if (mysql_query(mysql, sql_string)) {
    printf("error: %sn", mysql_error(mysql));
    return NULL;
  }

  g_res = mysql_use_result(mysql);
  return g_res;
}

MYSQL_ROW CSensor::fetchRow(MYSQL_RES *res){
  return mysql_fetch_row(res);
}

void CSensor::freeResult(MYSQL_RES *res){
  mysql_free_result(res);
  g_res = NULL;
}
