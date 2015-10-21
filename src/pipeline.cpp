/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Moinuddin K. Qureshi
 * Date         : 19th February 2014
 * Description  : Out of Order Pipeline for Lab3 ECE 6100

 * Update       : Shravan Ramani, Tushar Krishna, 27th Sept, 2015
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>
#include <cstring>


extern int32_t PIPE_WIDTH;
extern int32_t SCHED_POLICY;
extern int32_t LOAD_EXE_CYCLES;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Inst
 **********************************************************************/

void pipe_fetch_inst(Pipeline *p, Pipe_Latch* fe_latch){
    static int halt_fetch = 0;
    uint8_t bytes_read = 0;
    Trace_Rec trace;
    if(halt_fetch != 1) {
      bytes_read = fread(&trace, 1, sizeof(Trace_Rec), p->tr_file);
      Inst_Info *fetch_inst = &(fe_latch->inst);
    // check for end of trace
    // Send out a dummy terminate op
      if( bytes_read < sizeof(Trace_Rec)) {
        p->halt_inst_num=p->inst_num_tracker;
        halt_fetch = 1;
        fe_latch->valid=true;
        fe_latch->inst.dest_reg = -1;
        fe_latch->inst.src1_reg = -1;
        fe_latch->inst.src1_reg = -1;
        fe_latch->inst.inst_num=-1;
        fe_latch->inst.op_type=4;
        return;
      }

    // got an instruction ... hooray!
      fe_latch->valid=true;
      fe_latch->stall=false;
      p->inst_num_tracker++;
      fetch_inst->inst_num=p->inst_num_tracker;
      fetch_inst->op_type=trace.op_type;

      fetch_inst->dest_reg=trace.dest_needed? trace.dest:-1;
      fetch_inst->src1_reg=trace.src1_needed? trace.src1_reg:-1;
      fetch_inst->src2_reg=trace.src2_needed? trace.src2_reg:-1;

      fetch_inst->dr_tag=-1;
      fetch_inst->src1_tag=-1;
      fetch_inst->src2_tag=-1;
      fetch_inst->src1_ready=false;
      fetch_inst->src2_ready=false;
      fetch_inst->exe_wait_cycles=0;
    } else {
      fe_latch->valid = false;
    }
    return;
}


/**********************************************************************
 * Pipeline Class Member Functions
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->pipe_RAT=RAT_init();
    p->pipe_ROB=ROB_init();
    p->pipe_EXEQ=EXEQ_init();
    p->tr_file = tr_file_in;
    p->halt_inst_num = ((uint64_t)-1) - 3;
    int ii =0;
    for(ii = 0; ii < PIPE_WIDTH; ii++) {  // Loop over No of Pipes
      p->FE_latch[ii].valid = false;
      p->ID_latch[ii].valid = false;
      p->EX_latch[ii].valid = false;
      p->SC_latch[ii].valid = false;
    }
    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;
    uint8_t latch_type_i = 0;
    uint8_t width_i      = 0;
   for(latch_type_i = 0; latch_type_i < 4; latch_type_i++) {
        switch(latch_type_i) {
        case 0:
            printf(" FE: ");
            break;
        case 1:
            printf(" ID: ");
            break;
        case 2:
            printf(" SCH: ");
            break;
        case 3:
            printf(" EX: ");
            break;
        default:
            printf(" -- ");
          }
    }
   printf("\n");
   for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
       if(p->FE_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->FE_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->ID_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->ID_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->SC_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->SC_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->EX_latch[width_i].valid == true) {
         for(int ii = 0; ii < MAX_WRITEBACKS; ii++) {
            if(p->EX_latch[ii].valid)
	      printf("  %d  ", (int)p->EX_latch[ii].inst.inst_num);
         }
       } else {
         printf(" --  ");
       }
        printf("\n");
     }
     printf("\n");

     RAT_print_state(p->pipe_RAT);
     EXEQ_print_state(p->pipe_EXEQ);
     ROB_print_state(p->pipe_ROB);
}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_commit(p);
    pipe_cycle_writeback(p);
    pipe_cycle_exe(p);
    pipe_cycle_schedule(p);
    pipe_cycle_issue(p);
    pipe_cycle_decode(p);
    pipe_cycle_fetch(p);
//    ROB_print_state(p->pipe_ROB);
//    EXEQ_print_state(p->pipe_EXEQ);
    //RAT_print_state(p->pipe_RAT);
//    pipe_print_state(p);
   }

//--------------------------------------------------------------------//

void pipe_cycle_fetch(Pipeline *p){
  int ii = 0;
  Pipe_Latch fetch_latch;

  for(ii=0; ii<PIPE_WIDTH; ii++) {
    if((p->FE_latch[ii].stall) || (p->FE_latch[ii].valid)) {   // Stall
        continue;

    } else {  // No Stall and Latch Empty
        pipe_fetch_inst(p, &fetch_latch);
        // copy the op in FE LATCH
        p->FE_latch[ii]=fetch_latch;
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_decode(Pipeline *p){
   int ii = 0;

   int jj = 0;

   static uint64_t start_inst_id = 1;

   // Loop Over ID Latch
   for(ii=0; ii<PIPE_WIDTH; ii++){
     if((p->ID_latch[ii].stall == 1) || (p->ID_latch[ii].valid)) { // Stall
       continue;
     } else {  // No Stall & there is Space in Latch
       for(jj = 0; jj < PIPE_WIDTH; jj++) { // Loop Over FE Latch
         if(p->FE_latch[jj].valid) {
           if(p->FE_latch[jj].inst.inst_num == start_inst_id) { // In Order Inst Found
             p->ID_latch[ii]        = p->FE_latch[jj];
             p->ID_latch[ii].valid  = true;
             p->FE_latch[jj].valid  = false;
             start_inst_id++;
             break;
           }
         }
       }
     }
   }
}

//--------------------------------------------------------------------//

void pipe_cycle_exe(Pipeline *p){

  int ii;
  //If all operations are single cycle, simply copy SC latches to EX latches
  if(LOAD_EXE_CYCLES == 1) {
    for(ii=0; ii<PIPE_WIDTH; ii++){
      if(p->SC_latch[ii].valid) {
        p->EX_latch[ii]=p->SC_latch[ii];
        p->EX_latch[ii].valid = true;
        p->SC_latch[ii].valid = false;
      }
      return;
    }
  }

  //---------Handling exe for multicycle operations is complex, and uses EXEQ

  // All valid entries from SC get into exeq

  for(ii = 0; ii < PIPE_WIDTH; ii++) {
    if(p->SC_latch[ii].valid) {
      EXEQ_insert(p->pipe_EXEQ, p->SC_latch[ii].inst);
      p->SC_latch[ii].valid = false;
    }
  }

  // Cycle the exeq, to reduce wait time for each inst by 1 cycle
  EXEQ_cycle(p->pipe_EXEQ);

  // Transfer all finished entries from EXEQ to EX_latch
  int index = 0;

  while(1) {
    if(EXEQ_check_done(p->pipe_EXEQ)) {
      p->EX_latch[index].valid = true;
      p->EX_latch[index].stall = false;
      p->EX_latch[index].inst  = EXEQ_remove(p->pipe_EXEQ);
      index++;
    } else { // No More Entry in EXEQ
      break;
    }
  }
}



/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_issue(Pipeline *p) {
  int ii = 0;
  // insert new instruction(s) into ROB (rename)
  // every cycle up to PIPEWIDTH instructions issued

  // Find space in ROB and transfer instruction (valid = 1, exec = 0, ready = 0)
  // If src1/src2 is not remapped, set src1ready/src2ready
  // If src1/src is remapped, set src1tag/src2tag from RAT. Set src1ready/src2ready based on ready bit from ROB entries.
  // Set dr_tag
  p->ID_latch[ii].stall = false;
  p->FE_latch[ii].stall = false;
  for(ii = 0; ii < PIPE_WIDTH; ii++){
    if(ROB_check_space(p->pipe_ROB) ){ 
      if(!p->ID_latch[ii].valid){
        return;
      }
      //get and insert src1/src2 remapping and set the ready bit
      if(p->ID_latch[ii].inst.src1_reg == -1 || RAT_get_remap(p->pipe_RAT, p->ID_latch[ii].inst.src1_reg) == -1){
        p->ID_latch[ii].inst.src1_ready = true;    //not in rat use arf
      }else{
        p->ID_latch[ii].inst.src1_tag = RAT_get_remap(p->pipe_RAT, p->ID_latch[ii].inst.src1_reg);
        p->ID_latch[ii].inst.src1_ready = ROB_check_ready(p->pipe_ROB, p->ID_latch[ii].inst.src1_tag);//using prf_id(tag) as an index check the rob to see if it's ready
      }
      if(p->ID_latch[ii].inst.src2_reg == -1 || RAT_get_remap(p->pipe_RAT, p->ID_latch[ii].inst.src2_reg) == -1){
        p->ID_latch[ii].inst.src2_ready = true;   //not in rat use arf
      }else{
        p->ID_latch[ii].inst.src2_tag = RAT_get_remap(p->pipe_RAT, p->ID_latch[ii].inst.src2_reg);
        p->ID_latch[ii].inst.src2_ready = ROB_check_ready(p->pipe_ROB, p->ID_latch[ii].inst.src2_tag); //using prf_id(tag) as an index check the rob to see if it's ready     
      }
      //set dr_tag
        //for setting dr_tag one possibility is check for space then set dr tag equal to tail_ptr
      p->ID_latch[ii].inst.dr_tag = p->pipe_ROB->tail_ptr;
      //insert into rob
      int prd_idx = ROB_insert(p->pipe_ROB, p->ID_latch[ii].inst); //the return value for this is a param for set_remap in RAT
      //make sure that dest reg is needed (-1 if not needed)
      if(p->ID_latch[ii].inst.dest_reg != -1)
        RAT_set_remap(p->pipe_RAT, p->ID_latch[ii].inst.dest_reg, prd_idx);
      p->ID_latch[ii].valid = false;
    }else{
      //if rob is full stall...............
      p->ID_latch[ii].stall = true;
      p->FE_latch[ii].stall = true;
      return;
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_schedule(Pipeline *p) {
  int ii = 0;
  // select instruction(s) to Execute
  // every cycle up to PIPEWIDTH instructions scheduled

  // Implement two scheduling policies (SCHED_POLICY: 0 and 1)

  if(SCHED_POLICY==0){
    // inorder scheduling
    // Find all valid entries, if oldest is stalled then stop
    // Else mark it as ready to execute and send to SC_latch
      // if both sources are ready and valid then it becomes scheduled and exec = 1
  int idx = p->pipe_ROB->head_ptr;
   for(int i = 0; i<MAX_ROB_ENTRIES; i++){ 
    if(p->pipe_ROB->ROB_Entries[idx].valid && p->pipe_ROB->ROB_Entries[idx].inst.src1_ready && p->pipe_ROB->ROB_Entries[idx].inst.src2_ready && !p->pipe_ROB->ROB_Entries[idx].exec && !p->pipe_ROB->ROB_Entries[idx].ready){
      ROB_mark_exec(p->pipe_ROB, p->pipe_ROB->ROB_Entries[idx].inst); // this second param is really convoluted and I think unneeded
      p->SC_latch[ii].inst = p->pipe_ROB->ROB_Entries[idx].inst;
      p->SC_latch[ii].valid = true;
      p->SC_latch[ii].stall = false;
      return; //for n= 1
    }
    if(!p->pipe_ROB->ROB_Entries[idx].exec){//hasn't executed and sources must not be ready so stop looking this is oldest and isn't ready
      return;
    }
    if(idx >= MAX_ROB_ENTRIES-1)
      idx = -1;
    idx++;
   }
  }

  if(SCHED_POLICY==1){
    // out of order scheduling
    // Find valid + src1ready + src2ready + !exec entries in ROB
    // Mark ROB entry as ready to execute  and transfer instruction to SC_latch
      //loop from head to tail checking bits
   for(int idx = p->pipe_ROB->head_ptr; idx != p->pipe_ROB->tail_ptr; idx++){
    if(p->pipe_ROB->ROB_Entries[idx].valid && p->pipe_ROB->ROB_Entries[idx].inst.src1_ready && p->pipe_ROB->ROB_Entries[idx].inst.src2_ready && !p->pipe_ROB->ROB_Entries[idx].exec && !p->pipe_ROB->ROB_Entries[idx].ready){
      ROB_mark_exec(p->pipe_ROB, p->pipe_ROB->ROB_Entries[idx].inst); // this second param is really convoluted and I think unneeded
      p->SC_latch[ii].inst = p->pipe_ROB->ROB_Entries[idx].inst;
      p->SC_latch[ii].valid = true;
      p->SC_latch[ii].stall = false;
      return; //for n= 1
    }
    //make idx circular
    if(idx >= MAX_ROB_ENTRIES-1)
      idx = -1;
    } 
  }
}


//--------------------------------------------------------------------//

void pipe_cycle_writeback(Pipeline *p){
  int ii = 0;
  //  Go through all instructions out of EXE latch
  //  Writeback to ROB (using wakeup function)
  //  Update the ROB, mark ready, and update Inst Info in ROB
  for(ii = 0; ii < MAX_WRITEBACKS; ii++){ //loop over all of the entries in the exe latch
    if(p->EX_latch[ii].valid == true){ // needed for the latency thing
      ROB_wakeup(p->pipe_ROB, p->EX_latch[ii].inst.dr_tag);
      ROB_mark_ready(p->pipe_ROB, p->EX_latch[ii].inst);
      p->EX_latch[ii].valid = false; //mark invalid to ensure that latch is only read once
    }
  }
}


//--------------------------------------------------------------------//


void pipe_cycle_commit(Pipeline *p) {
  int ii = 0;
  //  check the head of the ROB. If ready commit (update stats)
  //  Deallocate entry from ROB
  //  Update RAT after checking if the mapping is still relevant
  if(ROB_check_head(p->pipe_ROB)){
    Inst_Info inst = ROB_remove_head(p->pipe_ROB);
    //update rat with the info in inst
    if(inst.dest_reg != -1 && inst.dr_tag == (int)p->pipe_RAT->RAT_Entries[inst.dest_reg].prf_id) //if dest wasn't used then don't mess with rat and if rat has since been changed dont invalidate
      RAT_reset_entry(p->pipe_RAT, inst.dest_reg);
    p->stat_retired_inst++;
    if(p->pipe_ROB->head_ptr == p->pipe_ROB->tail_ptr )
      p->halt=true;   
  }
  if(p->FE_latch[ii].inst.inst_num >= p->halt_inst_num)//stop fetching when done
    p->FE_latch[ii].valid=false;
}

//--------------------------------------------------------------------//



