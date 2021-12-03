#ifndef _CSENSOR_TEST_H_
#define _CSENSOR_TEST_H_

/*
 *  csensor-witmotion.h
 *  qcn
 *
 *  Created by Carl Christensen on 08/11/2007.
 *  Copyright 2007 Stanford University
 *
 * This file contains an example usage of the CSensor class for QCN
 */

#include "csensor.h"

#define STR_LINUX_USB_WITMOTION01     "/dev/ttyUSB*"

// this is an example declaration of a class derived from CSensor
// you probably will not need to modify CSensor -- just override the necessary functions here
class CSensorWitMotion  : public CSensor
{
   private:
        // private member vars if needed
        int m_fd;
        char r_buf[1024];

        // you will need to define a read_xyz function (pure virtual function in CSensor)
        virtual bool read_xyz(float& x1, float& y1, float& z1);

   public:
        // public member vars if needed

        CSensorWitMotion();
        virtual ~CSensorWitMotion();

        virtual void closePort(); // closes the port if open
        virtual bool detect();   // this detects & initializes the sensor
        //virtual int uartOpen(int fd,const char *pathname);
        //virtual int uartSet(int fd,int nSpeed, int nBits, char nEvent, int nStop);
        virtual int send_data(char *send_buffer, int length);
        virtual int recv_data(char* recv_buffer, int length);

        // note that CSensor defines a mean_xyz method that uses the read_xyz declared above -- you shouldn't need to override mean_xyz but that option is there
};

#endif

