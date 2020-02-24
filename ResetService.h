/*
 * ResetService.h
 *
 *  Created on: 27 Jul 2019
 *      Author: stefanosperett
 */

#ifndef RESETSERVICE_H_
#define RESETSERVICE_H_

#include "Service.h"
#include "DSerial.h"
#include "ResetCodes.h"

#define RESET_SERVICE           19
#define RESET_ERROR              0
#define RESET_REQUEST            1
#define RESET_RESPONSE           2

#define RESET_SOFT               1
#define RESET_HARD               2
#define RESET_POWERCYCLE         3

class ResetService: public Service
{
 protected:
     const unsigned long WDIPort;
     const unsigned long WDIPin;

//     uint32_t softResetStat;
//     uint32_t hardResetStat;
//     uint32_t pssResetStat;
//     uint32_t pcmResetStat;
//     uint32_t pinResetStat;
//     uint32_t rebootResetStat;
//     uint32_t csResetStat;

     uint32_t resetStatus = 0;
     uint32_t csStatus = 0;
     uint32_t csFaults = 0;


 public:
     ResetService( const unsigned long port, const unsigned long pin );
     virtual bool process( DataMessage &command, DataMessage &workingBbuffer );
     void init();
     uint32_t getResetStatus();
     void readResetStatus();
     void readCSStatus();
     void refreshConfiguration();
     void kickExternalWatchDog();
     void kickInternalWatchDog();

     void forceHardReset();
     void forceSoftReset();
};

#endif /* RESETSERVICE_H_ */
