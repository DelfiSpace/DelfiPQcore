/*
 * SoftwareUpdateService.cpp
 *
 *  Created on: 27 Jul 2019
 *      Author: Jasper Haenen
 */

#include "SoftwareUpdateService.h"

extern DSerial serial;

/**
 *
 *   Process the Service (Called by CommandHandler)
 *
 *   Parameters:
 *   PQ9Frame &command          Frame received over the bus
 *   DataBus &interface       Bus object
 *   PQ9Frame &workingBuffer    Reference to buffer to store the response.
 *
 *   Returns:
 *   bool true      :           Frame is directed to this Service
 *        false     :           Frame is not directed to this Service
 *
 */

SoftwareUpdateService::SoftwareUpdateService(MB85RS &fram_in) {
    fram = &fram_in;
}

bool SoftwareUpdateService::process(DataMessage &command, DataMessage &workingBuffer) {
    if (command.getPayload()[COMMAND_SERVICE] == SOFTWAREUPDATE_SERVICE) {
        // prepare response frame
//        workingBuffer.setDestination(command.getSource());
//        workingBuffer.setSource(interface.getAddress());
        workingBuffer.setSize(PAYLOAD_SIZE_OFFSET);
        workingBuffer.getPayload()[COMMAND_SERVICE] = SOFTWAREUPDATE_SERVICE;
        workingBuffer.getPayload()[COMMAND_RESPONSE] = COMMAND_REPLY;
        workingBuffer.getPayload()[COMMAND_METHOD] = command.getPayload()[COMMAND_METHOD];

        payload_data = workingBuffer.getPayload();
        payload_size = PAYLOAD_SIZE_OFFSET;

        if(command.getPayload()[COMMAND_METHOD] != ERASE_SLOT) state_flags &= ~ERASE_FLAG;

        switch (command.getPayload()[COMMAND_METHOD]) {
        case START_OTA:
            if((command.getSize() == PAYLOAD_SIZE_OFFSET + 1) || (command.getSize() == PAYLOAD_SIZE_OFFSET + 2)){
                if(command.getPayload()[COMMAND_DATA] == 1 || command.getPayload()[COMMAND_DATA] == 2) {
                    if(command.getSize() == PAYLOAD_SIZE_OFFSET + 2){
                        start_OTA(command.getPayload()[COMMAND_DATA], command.getPayload()[COMMAND_DATA+1] == 1);
                    }else{
                        start_OTA(command.getPayload()[COMMAND_DATA], false);
                    }
                    if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nOTA started!");

                } else throw_error(SLOT_OUT_OF_RANGE);
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case SET_METADATA:
            if(command.getSize() == METADATA_SIZE - 1 + PAYLOAD_SIZE_OFFSET) {
                receive_metadata(&(command.getPayload()[COMMAND_DATA]));
                if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nMetadata received!");

            } else throw_error(PARAMETER_MISMATCH);
            break;

        case GET_METADATA:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET + 1) {
                if(command.getPayload()[COMMAND_DATA] == 1 || command.getPayload()[COMMAND_DATA] == 2) {
                    send_metadata(command.getPayload()[COMMAND_DATA] - 1);
                    if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) {
                        print_metadata(&payload_data[COMMAND_DATA]);
                        serial.println("\nMetadata sended!");
                    }
                } else throw_error(SLOT_OUT_OF_RANGE);
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case RECEIVE_PARTIAL_CRCS:
            if(command.getSize() <= BLOCK_SIZE + PAYLOAD_SIZE_OFFSET + 2) { //2 extra bytes for offset bytes
//                serial.print("SIZE BYTE msB: ");
//                serial.println(command.getPayload()[COMMAND_DATA + 1],DEC);
//                serial.print("SIZE BYTE lsB: ");
//                serial.println(command.getPayload()[COMMAND_DATA],DEC);
//                serial.print("CRC OFFSET: ");
//                serial.println(command.getPayload()[COMMAND_DATA] | (command.getPayload()[COMMAND_DATA + 1] << 8), DEC);
                receive_partial_crcs(&(command.getPayload()[COMMAND_DATA+2]), command.getSize() - (PAYLOAD_SIZE_OFFSET+2), command.getPayload()[COMMAND_DATA] | (command.getPayload()[COMMAND_DATA + 1] << 8));
                if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nPartial crc block received!");
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case RECEIVE_BLOCK:
            if(command.getSize() <= BLOCK_SIZE + 2 + PAYLOAD_SIZE_OFFSET) {
                receive_block(&(command.getPayload()[COMMAND_DATA + 2]), command.getPayload()[COMMAND_DATA] | (command.getPayload()[COMMAND_DATA + 1] << 8));
                if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nBlock received!");
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case CHECK_MD5:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET + 1) {
                if(command.getPayload()[COMMAND_DATA] == 1 || command.getPayload()[COMMAND_DATA] == 2) {
                    check_md5(command.getPayload()[COMMAND_DATA]);
                    if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nMD5 is correct!");
                } else throw_error(SLOT_OUT_OF_RANGE);
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case STOP_OTA:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET) {
                stop_OTA();
                if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nOTA is stopped!");
            } else throw_error(PARAMETER_MISMATCH);
            break;

        case ERASE_SLOT:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET + 1) {
                if((state_flags & ERASE_FLAG) == 0) {
                    if(command.getPayload()[COMMAND_DATA] == 1 || command.getPayload()[COMMAND_DATA] == 2) {
                        slot_erase = command.getPayload()[COMMAND_DATA];
                        state_flags |= ERASE_FLAG;
                        serial.println("Are you sure(13)?");
                    } else throw_error(SLOT_OUT_OF_RANGE);
                } else {
                    if(command.getPayload()[COMMAND_DATA] == ACKNOWLEDGE) {
                        erase_slot(slot_erase);
                        if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nSlot is erased!");
                    } else throw_error(PARAMETER_MISMATCH);
                }
            } else throw_error(PARAMETER_MISMATCH);
            break;
        case SET_BOOT_SLOT:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET + 2) {
                if(command.getPayload()[COMMAND_DATA] < 3) {
                    set_boot_slot(command.getPayload()[COMMAND_DATA], command.getPayload()[COMMAND_DATA + 1]);
                    if(payload_data[COMMAND_RESPONSE] != COMMAND_ERROR) serial.println("\nSlot code executed successfully!");
                } else throw_error(SLOT_OUT_OF_RANGE);
            } else throw_error(PARAMETER_MISMATCH);
            break;
        case GET_MISSED_BLOCKS:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET) {
                get_missed_blocks();
            } else throw_error(PARAMETER_MISMATCH);
            break;
        case GET_MISSED_CRC:
            if(command.getSize() == PAYLOAD_SIZE_OFFSET) {
                get_missed_crc();
            } else throw_error(PARAMETER_MISMATCH);
            break;
        default:
            break;
        }

        workingBuffer.setSize(payload_size);

        // command processed
        return true;
    } else {
        // this command is related to another service,
        // report the command was not processed
        return false;
    }
}

void SoftwareUpdateService::start_OTA(unsigned char slot_number, bool allow_resume) {
    payload_size = PAYLOAD_SIZE_OFFSET;
    uint8_t* current_slot = (uint8_t*)CURRENT_SLOT_ADDRESS;


    if(*current_slot != (slot_number)) { //Warning! Do not reprogram the current program, very bad idea

        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
        fram->read(UPDATE_PROGRESS_STATE, &state_flags, 1);

        //if((state_flags & UPDATE_FLAG) == 0) { //system is not already in update mode.
            unsigned char slotState;
            if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
            if(slot_number == 1){
                fram->read(SLOT1_METADATA_STATE, &slotState, 1);
            }else if(slot_number == 2){
                fram->read(SLOT2_METADATA_STATE, &slotState, 1);
            }
            serial.print("Slot State: ");
            serial.println(slotState, HEX);
            if((slotState & 0x03) == EMPTY) {
                serial.println("Slot is empty!");
                slotState = PARTIAL; //Set status to Partial OTA
                slotState |= UPDATE_FLAG;
                if(slot_number == 2){
                    slotState |= SLOT_SELECT_FLAG;
                }
                slotState &= ~(METADATA_FLAG | PARTIAL_CRC_FLAG | MD5_CORRECT_FLAG); //unset these flags, just to be sure

                update_slot = slot_number; //store my updateSlot in RAM
                state_flags = slotState; //store my update progress also in RAM

                //write my current update Status to FRAM
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
                if(slot_number == 1){
                    fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
                }else if(slot_number == 2){
                    fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
                }

                //initialize the checklists with zeros
                for(int i = 0; i < (MAX_BLOCK_AMOUNT/BYTE_SIZE); i++) crc_received[i] = 0;
                for(int i = 0; i < (MAX_BLOCK_AMOUNT/BYTE_SIZE); i++) blocks_received[i] = 0;
                //write the FRAM Progress checklists
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                fram->write(UPDATE_PROGRESS_CRC, crc_received, UPDATE_PROGRESS_CHECKLIST_SIZE);
                fram->write(UPDATE_PROGRESS_BLOCKS, blocks_received, UPDATE_PROGRESS_CHECKLIST_SIZE);

            } else if((slotState & 0x03) == PARTIAL) { //slot is not empty but partially updated
                // first check if the partially updated slot Aligns with current progress and check for resume:
                serial.print("Update still in progress, progress state: ");
                serial.print(state_flags, HEX);
                serial.print("  -  slot state: ");
                serial.println(slotState, HEX);
                if((slotState == state_flags) && ((state_flags & UPDATE_FLAG) == UPDATE_FLAG )){ //check if we can resume, due to the progress status being identical to the slot status:
                    if(allow_resume){ //if we allow resume
                        update_slot = slot_number; //store my updateSlot in RAM

                        //retrieve number of blocks from MetaData
                        unsigned char nrOfBlocksBuf[METADATA_NR_OF_BLOCKS_SIZE] = {0};
                        if(slot_number == 1){
                            fram->read(SLOT1_METADATA_NR_OF_BLOCKS, nrOfBlocksBuf, METADATA_NR_OF_BLOCKS_SIZE);
                        }else if(slot_number == 2){
                            fram->read(SLOT2_METADATA_NR_OF_BLOCKS, nrOfBlocksBuf, METADATA_NR_OF_BLOCKS_SIZE);
                        }

                        //get nr of blocks
                        num_update_blocks = nrOfBlocksBuf[0] | (nrOfBlocksBuf[1] << 8);

                        //read the FRAM Progress checklists
                        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                        fram->read(UPDATE_PROGRESS_CRC, crc_received, UPDATE_PROGRESS_CHECKLIST_SIZE);
                        fram->read(UPDATE_PROGRESS_BLOCKS, blocks_received, UPDATE_PROGRESS_CHECKLIST_SIZE);

                        //write my current update Status to FRAM
                        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                        fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
                        if(slot_number == 1){
                            fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
                        }else if(slot_number == 2){
                            fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
                        }
                        serial.println("Update Resumed!!");
                    }else{ // decided not to resume but slot is currently updating
                        return throw_error(UPDATE_ALREADY_STARTED);
                    }
                }else{ // either impossible to resume since its a different slot, or not in-update on non-empty slot
                    return throw_error(SLOT_NOT_EMPTY);
                }
            } else return throw_error(SLOT_NOT_EMPTY);
        //} else return throw_error(UPDATE_ALREADY_STARTED);
    } else return throw_error(SELF_ACTION);
}

void SoftwareUpdateService::receive_metadata(unsigned char* metadata) {
    payload_size = PAYLOAD_SIZE_OFFSET;

    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) { //if state is in-Update
        if((state_flags & METADATA_FLAG) == 0) { //if METADATA has not yet been received
            unsigned short temp_num_blocks = metadata[NUM_BLOCKS_OFFSET - 1] | (metadata[NUM_BLOCKS_OFFSET] << 8);
            if(temp_num_blocks <= MAX_BLOCK_AMOUNT) { //check if the metadata block amount is 'correct'

                //set METADATA in FRAM except for the state byte
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                if(update_slot == 1){
                    fram->write(SLOT1_METADATA_MD5, metadata, METADATA_SIZE - 1);
                }else if(update_slot == 2){
                    fram->write(SLOT2_METADATA_MD5, metadata, METADATA_SIZE - 1);
                }

                //update stateByte and NR of Blocks
                num_update_blocks = temp_num_blocks;
                state_flags |= METADATA_FLAG;   //set MetaData received Flag
                //write updates of flag to FRAM
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
                if(update_slot == 1){
                    fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
                }else if(update_slot == 2){
                    fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
                }
                serial.print("METADATA RECEIVED, Status: ");
                serial.println(state_flags, HEX);

            } else return throw_error(UPDATE_TO_BIG);
        } else return throw_error(METADATA_ALREADY_RECEIVED);
    } else return throw_error(UPDATE_NOT_STARTED);
}

void SoftwareUpdateService::send_metadata(unsigned char slot_number) {
    payload_size = PAYLOAD_SIZE_OFFSET + METADATA_SIZE ;

    if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
    if(slot_number == 1){
        fram->read(SLOT1_METADATA, &payload_data[COMMAND_DATA], METADATA_SIZE);
    }else if(slot_number == 2){
        fram->read(SLOT2_METADATA, &payload_data[COMMAND_DATA], METADATA_SIZE);
    }
}

void SoftwareUpdateService::receive_partial_crcs(unsigned char* crc_block, unsigned char num_bytes, unsigned short crc_offset) {
    payload_size = PAYLOAD_SIZE_OFFSET;
//    serial.println(crc_offset, DEC);
    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) { //update is active
        if((state_flags & METADATA_FLAG) ==  METADATA_FLAG) { //metaData has been received
            if(crc_offset + num_bytes <= num_update_blocks) { //the amount of CRCs received do not exceed the number of expected CRCs

                //write CRCs to FRAM
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                if(update_slot == 1){
                    fram->write(SLOT1_PAR_CRC + crc_offset, crc_block, num_bytes);
                }else if(update_slot == 2){
                    fram->write(SLOT2_PAR_CRC + crc_offset, crc_block, num_bytes);
                }

                serial.print("Writing CRCs: ");
                serial.println(num_bytes, DEC);
                //update checklist in RAM
                for(int k = 0; k < num_bytes; k++){
                    crc_received[(crc_offset+k) / BYTE_SIZE] |= 1 << ((crc_offset+k) % BYTE_SIZE);
                }

                //write checklist changes to FRAM
                if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                serial.print("Writing CRC CheckList Bytes: ");
                serial.println(((num_bytes-1)/BYTE_SIZE) + 1, DEC);
                for(int i = 0; i < (((num_bytes-1)/BYTE_SIZE) + 1); i++){
                    serial.println(crc_received[(crc_offset) / BYTE_SIZE + i], DEC);
                }
                fram->write(UPDATE_PROGRESS_CRC + (crc_offset / BYTE_SIZE), &crc_received[(crc_offset) / BYTE_SIZE], ((num_bytes-1)/BYTE_SIZE) + 1);

                //Check if all CRCs are received
                if(this->get_num_missed_crc() == 0){
                    serial.println("ALL CRCS RECEIVED!!");
                    //set Flag
                    state_flags |= PARTIAL_CRC_FLAG;
                    if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                    fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
                    if(update_slot == 1){
                        fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
                    }else if(update_slot == 2){
                        fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
                    }
                }
            } else return throw_error(PARAMETER_MISMATCH);
        } else return throw_error(METADATA_NOT_RECEIVED);
    } else return throw_error(UPDATE_NOT_STARTED);
}

void SoftwareUpdateService::receive_block(unsigned char* data_block, uint16_t block_offset) {
    payload_size = PAYLOAD_SIZE_OFFSET;

    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) { //update is started
        if((state_flags & METADATA_FLAG) == METADATA_FLAG) { //metaData has been received
            if((state_flags & PARTIAL_CRC_FLAG) == PARTIAL_CRC_FLAG) { //all CRCs are received
                if(block_offset <= num_update_blocks) { //if received block is within expected range

                    //check if CRC matches
                    if(check_partial_crc(data_block, block_offset)) {
                        //write block to FLASH
                        unsigned int sector =  1 << (((block_offset * BLOCK_SIZE + (update_slot - 1) * SLOT_SIZE)) / SECTOR_SIZE);
                        if(!MAP_FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, sector)) return throw_error(NO_SLOT_ACCESS);
                        if(!MAP_FlashCtl_programMemory(data_block, (void*)(BANK1_ADDRESS + (update_slot - 1) * SLOT_SIZE + block_offset * BLOCK_SIZE), BLOCK_SIZE)) return throw_error(NO_SLOT_ACCESS);
                        if(!MAP_FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, sector)) return throw_error(NO_SLOT_ACCESS);

                        //update checklist in RAM
                        blocks_received[block_offset / BYTE_SIZE] |= (1 << (block_offset) % BYTE_SIZE);

                        //write checklist changes to FRAM
                        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
                        fram->write(UPDATE_PROGRESS_BLOCKS + (block_offset / BYTE_SIZE), &blocks_received[block_offset / BYTE_SIZE], 1);

                    } else {
                        return throw_error(CRC_MISMATCH);
                    }
                } else return throw_error(OFFSET_OUT_OF_RANGE);
            } else return throw_error(PARTIAL_ALREADY_RECEIVED);
        } else return throw_error(METADATA_NOT_RECEIVED);
    } else return throw_error(UPDATE_NOT_STARTED);
}

bool SoftwareUpdateService::check_partial_crc(unsigned char* data_block, uint16_t block_offset) {
    unsigned char val = 0;

    for(int i = 0; i < BLOCK_SIZE; i++) {
        val = CRC_TABLE[val ^ data_block[i]];
    }

    unsigned char crc;
    if(!fram->ping()) {
        throw_error(NO_FRAM_ACCESS);
        return false;
    }
    //read CRC
    if(update_slot == 1){
        fram->read(SLOT1_PAR_CRC + block_offset, &crc, 1);
    }else if(update_slot == 2){
        fram->read(SLOT2_PAR_CRC + block_offset, &crc, 1);
    }


    if(crc != val) {
        serial.print("Block offset: ");
        serial.println(block_offset, DEC);
        serial.print("Calculated CRC: ");
        serial.println(val, DEC);
        serial.print("Stored CRC: ");
        serial.println(crc, DEC);
    }

    return crc == val;
}

void SoftwareUpdateService::check_md5(unsigned char slot_number) {
    payload_size = PAYLOAD_SIZE_OFFSET + 1;

    unsigned char digest[MD5_SIZE];

    MD5_CTX md5_c;
    MD5_Init(&md5_c);

    uint16_t num_blocks;
//    if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
//    fram->read((METADATA_SIZE + PAR_CRC_SIZE) * slot_number + NUM_BLOCKS_OFFSET, (unsigned char*)&num_blocks, sizeof(uint16_t));
    unsigned char nrOfBlocksBuf[METADATA_NR_OF_BLOCKS_SIZE] = {0};
    if(slot_number == 1){
        fram->read(SLOT1_METADATA_NR_OF_BLOCKS, nrOfBlocksBuf, METADATA_NR_OF_BLOCKS_SIZE);
    }else if(slot_number == 2){
        fram->read(SLOT2_METADATA_NR_OF_BLOCKS, nrOfBlocksBuf, METADATA_NR_OF_BLOCKS_SIZE);
    }

    //get nr of blocks
    num_blocks = nrOfBlocksBuf[0] | (nrOfBlocksBuf[1] << 8);

    unsigned char meta_crc[MD5_SIZE];
    if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
    if(slot_number == 1){
        fram->read(SLOT1_METADATA_MD5, meta_crc, MD5_SIZE);
    }else if(slot_number == 2){
        fram->read(SLOT2_METADATA_MD5, meta_crc, MD5_SIZE);
    }

    MD5_Update(&md5_c, (unsigned char*)(BANK1_ADDRESS + (slot_number - 1) * SLOT_SIZE), num_blocks * BLOCK_SIZE);

    MD5_Final(digest, &md5_c);

    bool equal = true;
    for(int i = 0; i < MD5_SIZE; i++) {
        if(digest[i] != meta_crc[i]) {
            equal = false;
            break;
        }
    }

    if(equal){
        state_flags |= MD5_CORRECT_FLAG;
        serial.println("MD5 Correct!");
    }
    if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
    fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
    if(slot_number == 1){
        fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
    }else if(slot_number == 2){
        fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
    }
    payload_data[COMMAND_DATA] = equal;
}

void SoftwareUpdateService::stop_OTA() {
    payload_size = PAYLOAD_SIZE_OFFSET + 1;
    serial.print("stopping update, status: ");
    serial.println(state_flags, HEX);
    check_md5(update_slot);

    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) { //OTA is running, time to kill it.
        state_flags &= ~UPDATE_FLAG;
        if((state_flags & MD5_CORRECT_FLAG) == MD5_CORRECT_FLAG){ //MD5 was deemed correct. So set status to FULL
            serial.println("MD5 is Correct!");
            state_flags &= ~0x03;
            state_flags |= FULL;
        }
        serial.print("stopped update, status: ");
        serial.println(state_flags, HEX);
        //Write State
        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
        if(update_slot == 1){
            fram->write(SLOT1_METADATA_STATE, &state_flags, 1);
        }else if(update_slot == 2){
            fram->write(SLOT2_METADATA_STATE, &state_flags, 1);
        }

        state_flags = 0; //destroy progress flags
        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
        fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);

    } else return throw_error(UPDATE_NOT_STARTED);
}

void SoftwareUpdateService::erase_slot(unsigned char slot) {
    payload_size = PAYLOAD_SIZE_OFFSET + 1;

    uint8_t* current_slot = (uint8_t*)CURRENT_SLOT_ADDRESS;
    if(*current_slot != slot ) { //dont erase yourself, very bad idea
        if((state_flags & UPDATE_FLAG) == 0) { //check if update is not in progress
            //delete MetaData
           unsigned char empty[METADATA_SIZE] = { 0 };
           if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
           if(slot == 1){
               fram->write(SLOT1_METADATA, empty, METADATA_SIZE);
           }else if(slot == 2){
               fram->write(SLOT2_METADATA, empty, METADATA_SIZE);
           }

           //mass erase flash
           if(!MAP_FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, 0xFFFF << (16 * (slot - 1)))) return throw_error(NO_SLOT_ACCESS);
           if(!MAP_FlashCtl_performMassErase()) return throw_error(NO_SLOT_ACCESS);
           if(!MAP_FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, 0xFFFF << (16 * (slot - 1)))) return throw_error(NO_SLOT_ACCESS);

           state_flags = 0; //destroy all flags
           if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
           fram->write(UPDATE_PROGRESS_STATE, &state_flags, 1);
        } else return throw_error(UPDATE_ALREADY_STARTED);
    } else return throw_error(SELF_ACTION);
}

void SoftwareUpdateService::set_boot_slot(unsigned char slot, bool permanent) {
    uint8_t* current_slot = (uint8_t*)CURRENT_SLOT_ADDRESS;
    if(slot == 0) { //if setting SLOT0 (fallback slot), no worries, just do it.
        uint8_t target_slot = (permanent) ? BOOT_PERMANENTLY : 0;
        fram->write(FRAM_TARGET_SLOT, &target_slot, 1);
        MAP_SysCtl_rebootDevice();
    } else {
        unsigned char slotFlag = 0;
        if(!fram->ping()) return throw_error(NO_FRAM_ACCESS);
        if(slot == 1){
            fram->read(SLOT1_METADATA_STATE, &slotFlag, 1);
        }else if(slot == 2){
            fram->read(SLOT2_METADATA_STATE, &slotFlag, 1);
        }
        serial.print("Status flag of Target: ");
        serial.println(slotFlag, HEX);

        //check_md5(slot); No reason to check again, flag should be set already.

        if((slotFlag & MD5_CORRECT_FLAG) == 0) return throw_error(MD5_MISMATCH);
        if((slotFlag & FULL) == FULL) {
            uint8_t target_slot = slot | ((permanent) ? BOOT_PERMANENTLY : 0);
            fram->write(FRAM_TARGET_SLOT, &target_slot, 1);
            MAP_SysCtl_rebootDevice();
        } else return throw_error(SLOT_NOT_PROGRAMMED);
    }
}

unsigned int SoftwareUpdateService::get_num_missed_blocks() {
    unsigned int count = 0;
    for(int missed_pointer = 0; missed_pointer < num_update_blocks; missed_pointer++) {
        if((blocks_received[missed_pointer / BYTE_SIZE] & (1 << missed_pointer % BYTE_SIZE)) == 0) { //there is a block missing here
           count++;
        }
    }
    return count;
}

unsigned int SoftwareUpdateService::get_num_missed_crc() {
    unsigned int count = 0;
    for(int missed_pointer = 0; missed_pointer < num_update_blocks; missed_pointer++) {
        if((crc_received[missed_pointer / BYTE_SIZE] & (1 << missed_pointer % BYTE_SIZE)) == 0) { //there is a block missing here
           count++;
        }
    }
    return count;
}

void SoftwareUpdateService::get_missed_blocks() {
    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) {

        switch(state_flags) {
            case FULL:
                serial.println("The requested slot is already fully programmed.");
                return;
            case EMPTY:
                return throw_error(UPDATE_NOT_STARTED);
            default:
                break;
        }

        int missed_pointer = 0;

        //get up to 32 missed blocks
        while(payload_size < (2*32+3)){

            if(missed_pointer == num_update_blocks) { //if everything is checked, just return
                missed_pointer = 0;
                return;
            }

            if((blocks_received[missed_pointer / BYTE_SIZE] & (1 << missed_pointer % BYTE_SIZE)) == 0) { //there is a block missing here
                memcpy(&payload_data[COMMAND_DATA + (payload_size - PAYLOAD_SIZE_OFFSET)], &missed_pointer, sizeof(uint16_t));
                payload_size += 2;
                serial.print("Block ");
                serial.print(missed_pointer, DEC);
                serial.println("  is missing.");
            }

            missed_pointer++;
        }
    } else return throw_error(UPDATE_NOT_STARTED);
}

void SoftwareUpdateService::get_missed_crc() {
    if((state_flags & UPDATE_FLAG) == UPDATE_FLAG) {

        switch(state_flags) {
            case FULL:
                serial.println("The requested slot is already fully programmed.");
                return;
            case EMPTY:
                return throw_error(UPDATE_NOT_STARTED);
            default:
                break;
        }

        int missed_pointer = 0;


        //get up to 32 missed blocks
        while(payload_size < (2*32+3)){

            int index = missed_pointer / BYTE_SIZE;

            if(missed_pointer == num_update_blocks) { //if everything is checked, just return
                missed_pointer = 0;
                return;
            }

            if((crc_received[missed_pointer / BYTE_SIZE] & (1 << missed_pointer % BYTE_SIZE)) == 0) { //there is a block missing here
                memcpy(&payload_data[COMMAND_DATA + (payload_size - PAYLOAD_SIZE_OFFSET)], &missed_pointer, sizeof(uint16_t));
                payload_size += 2;
                serial.print("CRC ");
                serial.print(missed_pointer, DEC);
                serial.println("  is missing.");
            }

            missed_pointer++;
        }
    } else return throw_error(UPDATE_NOT_STARTED);
}


void SoftwareUpdateService::print_metadata(unsigned char* metadata) {
    serial.println("Metadata:");
    serial.print("\tSlot status: ");
    switch (metadata[0])
    {
        case EMPTY:
            serial.println("Emtpy");
            break;
        case PARTIAL:
            serial.println("Partial");
            break;
        case FULL:
            serial.println("Full");
            break;
        default:
            serial.println("Unknown slot status!");
            break;
    }
    serial.print("\tVersion: ");
    if(metadata[MD5_SIZE+1] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+1], HEX);
    if(metadata[MD5_SIZE+2] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+2], HEX);
    if(metadata[MD5_SIZE+3] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+3], HEX);
    if(metadata[MD5_SIZE+4] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+4], HEX);
    if(metadata[MD5_SIZE+5] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+5], HEX);
    if(metadata[MD5_SIZE+6] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+6], HEX);
    if(metadata[MD5_SIZE+7] < 0x10) serial.print("0");
    serial.print(metadata[MD5_SIZE+7], HEX);
    if(metadata[MD5_SIZE+8] < 0x10) serial.print("0");
    serial.println(metadata[MD5_SIZE+8], HEX);
    serial.print("\tNumber of blocks: ");
    serial.println(metadata[MD5_SIZE+10] << 8 | metadata[MD5_SIZE+9], DEC);
    serial.print("\tMD5 CRC: ");
    for(int i = 0; i < MD5_SIZE; i++) {
        if(metadata[i + 1] < 0x10) serial.print("0");
        serial.print(metadata[i + 1], HEX);
    }
    serial.println();
}

void SoftwareUpdateService::throw_error(unsigned char error) {
    serial.print("Error(");
    serial.print(error, DEC);
    serial.print("): ");

    switch (error)
    {
    case NO_FRAM_ACCESS:
        serial.println("No access to FRAM.");
        break;
    case NO_SLOT_ACCESS:
        serial.println("No access to requested slot.");
        break;
    case SLOT_OUT_OF_RANGE:
        serial.println("Requested slot is out of range.");
        break;
    case MEMORY_FULL:
        serial.println("The memory is full.");
        break;
    case PARAMETER_MISMATCH:
        serial.println("Invalid parameter size provided to function.");
        break;
    case UPDATE_NOT_STARTED:
        serial.println("The update has not started yet.");
        break;
    case UPDATE_ALREADY_STARTED:
        serial.println("The update is still in progress.");
        break;
    case METADATA_ALREADY_RECEIVED:
        serial.println("The metadata has already been received.");
        break;
    case METADATA_NOT_RECEIVED:
        serial.println("The metadata has not been received yet.");
        break;
    case PARTIAL_ALREADY_RECEIVED:
        serial.println("The partial crcs have already been received.");
        break;
    case PARTIAL_NOT_RECEIVED:
        serial.println("The partial crcs have not been received yet.");
        break;
    case CRC_MISMATCH:
        serial.println("A partial crc mismatch has occurred.");
        break;
    case MD5_MISMATCH:
        serial.println("The md5 hash does not match.");
        break;
    case OFFSET_OUT_OF_RANGE:
        serial.println("The requested offset is out of range.");
        break;
    case SLOT_NOT_EMPTY:
        serial.println("The requested slot is not empty yet.");
        break;
    case UPDATE_TO_BIG:
        serial.println("The update is too big for the memory slot.");
        break;
    case SLOT_NOT_PROGRAMMED:
        serial.println("The requested slot is not (completely) programmed.");
        break;
    case UPDATE_NOT_CURRENT_SESSION:
        serial.println("The update cannot start, because the update is not from the current session. Erase the slot first and retry.");
        break;
    case SELF_ACTION:
        serial.println("The requested slot  cannot perform an action on itself.");
    default:
        break;
    }

    payload_data[COMMAND_RESPONSE] = COMMAND_ERROR;
    payload_size = PAYLOAD_SIZE_OFFSET + 1;
    payload_data[COMMAND_DATA] = error;
}
