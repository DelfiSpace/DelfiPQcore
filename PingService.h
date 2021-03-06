/*
 * PingService.h
 *
 *  Created on: 25 Jul 2019
 *      Author: stefanosperett
 */

#ifndef PINGSERVICE_H_
#define PINGSERVICE_H_

#include "Service.h"
#include "Console.h"

#define PING_SERVICE            17

class PingService: public Service
{
 public:
     virtual bool process( DataMessage &command, DataMessage &workingBbuffer );
};

#endif /* PINGSERVICE_H_ */
