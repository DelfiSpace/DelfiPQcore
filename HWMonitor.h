/*
 * HWMonitor.h
 *
 *  Created on: 18 Mar 2020
 *      Author: casperbroekhuizen
 */

#ifndef HWMONITOR_H_
#define HWMONITOR_H_


#include "ResetCodes.h"
#include "MB85RS.h"
#include "FRAMMap.h"
#include "Console.h"
#include "Bootloader.h"
#include "ADCManager.h"

class HWMonitor
{
 protected:
     MB85RS *fram = 0;
     bool hasFram = false;

     uint32_t resetStatus = 0;
     uint32_t CSStatus = 0;

     uint32_t cal30;
     uint32_t cal85;
     volatile float calDifference;

     uint16_t MCUTemp = 0;




 public:
     HWMonitor( MB85RS* fram  );
     HWMonitor();

     void readResetStatus();
     void readCSStatus();

     uint32_t getResetStatus();
     uint32_t getCSStatus();
     uint16_t getMCUTemp();

};



#endif /* HWMONITOR_H_ */
