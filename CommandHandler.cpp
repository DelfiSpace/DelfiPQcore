/*
 * Copyright (c) 2016 by Stefano Speretta <s.speretta@tudelft.nl>
 *
 * PQ9CommandHandler: a library to handle PQ9 commands.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * version 3, both as published by the Free Software Foundation.
 *
 */

#include <CommandHandler.h>

extern DSerial serial;

PQ9Bus *bus;
Service** servicesCH;
int servicesCount;

PQ9Frame rxBuffer, txBuffer;
PQ9CommandHandler *instance;

void stubCommandHandler()
{
    if (instance->handleCommands())
    {
        // command executed correctly
        //if (instance->onValidCmd)
        //{
        //    instance->onValidCmd();
        //}
    }
}

PQ9CommandHandler::PQ9CommandHandler(PQ9Bus &interface, Service **servArray, int count) :
          Task(stubCommandHandler)
{
    instance = this;
    onValidCmd = 0;
    bus = &interface;
    servicesCH = servArray;
    servicesCount = count;
}

void PQ9CommandHandler::received( PQ9Frame &newFrame )
{
    newFrame.copy(rxBuffer);
    notify();
}

void PQ9CommandHandler::onValidCommand(void (*function)( void ))
{
    onValidCmd = function;
}

bool PQ9CommandHandler::handleCommands()
{
    if (rxBuffer.getPayloadSize() > 1)
    {
        bool found = false;

        for (int i = 0; i < servicesCount; i++)
        {
            if (servicesCH[i]->process(rxBuffer, *bus, txBuffer))
            {
                // stop the loop if a service is found
                found = true;
                break;
            }
        }

        if (!found)
        {
            serial.print("Unknown Service (");
            serial.print(rxBuffer.getPayload()[0], DEC);
            serial.println(")");
            txBuffer.setDestination(rxBuffer.getSource());
            txBuffer.setSource(bus->getAddress());
            txBuffer.setPayloadSize(2);
            txBuffer.getPayload()[0] = 0;
            txBuffer.getPayload()[1] = 0;
            bus->transmit(txBuffer);
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        // invalid payload size
        // what should we do here?
        serial.println("Invalid Command, size must be > 1");
        txBuffer.setDestination(rxBuffer.getSource());
        txBuffer.setSource(bus->getAddress());
        txBuffer.setPayloadSize(2);
        txBuffer.getPayload()[0] = 0;
        txBuffer.getPayload()[1] = 0;
        bus->transmit(txBuffer);
        return false;
    }
}
