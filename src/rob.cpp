#include <stdio.h>
#include <assert.h>

#include "rob.h"

extern int32_t NUM_ROB_ENTRIES;

/////////////////////////////////////////////////////////////
// Init function initializes the ROB
/////////////////////////////////////////////////////////////

ROB* ROB_init(void){
  int ii;
  ROB *t = (ROB *) calloc (1, sizeof (ROB));
  for(ii=0; ii<MAX_ROB_ENTRIES; ii++){
    t->ROB_Entries[ii].valid=false;
    t->ROB_Entries[ii].ready=false;
    t->ROB_Entries[ii].exec=false;
  }
  t->head_ptr=0;
  t->tail_ptr=0;
  return t;
}

/////////////////////////////////////////////////////////////
// Print State
/////////////////////////////////////////////////////////////
void ROB_print_state(ROB *t){
 int ii = 0;
  printf("Printing ROB \n");
  printf("Entry  Inst   Valid   ready src1t   src2t\n");
  for(ii = 130; ii < 150; ii++) {
    printf("%5d ::  %d\t", ii, (int)t->ROB_Entries[ii].inst.inst_num);
    printf(" %5d\t", t->ROB_Entries[ii].valid);
    printf(" %5d\n", t->ROB_Entries[ii].ready);
    printf("ex %5d\n", t->ROB_Entries[ii].exec);
    printf(" %5d\n", t->ROB_Entries[ii].inst.src1_tag);
    printf(" %5d\n", t->ROB_Entries[ii].inst.src2_tag);
  }
  printf("\n");
}

/////////////////////////////////////////////////////////////
// If there is space in ROB return true, else false
/////////////////////////////////////////////////////////////

bool ROB_check_space(ROB *t){
//just check to see if tail is pointing to an invalid space...
  return (!(t->ROB_Entries[t->tail_ptr].valid) ); //if it points to invalid then there is space and true will be returned
}

/////////////////////////////////////////////////////////////
// insert entry at tail, increment tail (do check_space first)
/////////////////////////////////////////////////////////////

int ROB_insert(ROB *t, Inst_Info inst){
//from the handout
  //insert the instr, get remapped sources using rat, if rat is invalid mark the source as ready and if the rat is valid get the source ready bits from the rob
  //get the index(prf_id) of the rob entry/this is the new map for the dest reg
  //update the rat with the destination register map

//the return value is prd_id which is the index and should be -1 if the opperation is invalid
  if(!t->ROB_Entries[t->tail_ptr].valid){
    //insert inst
    inst.dr_tag = t->tail_ptr; //this sets the dr_tag as the prf_id
    t->ROB_Entries[t->tail_ptr].inst = inst;
    t->ROB_Entries[t->tail_ptr].valid = true;
    t->ROB_Entries[t->tail_ptr].exec = false;
    t->ROB_Entries[t->tail_ptr].ready = false;
    int index = t->tail_ptr;

 
//add more things like wtf is dr_tag i think it is the index of the rob? kana~
    //update the tail pointer
    t->tail_ptr++;
    if(t->tail_ptr >= MAX_ROB_ENTRIES)//makes the rob circular
      t->tail_ptr = 0;
    return index;
  }
  else{
    //no open space in the rob return -1
    return -1;
  }
}

/////////////////////////////////////////////////////////////
// When an inst gets scheduled for execution, mark exec
/////////////////////////////////////////////////////////////

void ROB_mark_exec(ROB *t, Inst_Info inst){
//look at the inst and mark the correct rob spot?
  //look at the dr_tag and mark that inst as exec
  t->ROB_Entries[inst.dr_tag].exec = true;
}


/////////////////////////////////////////////////////////////
// Once an instruction finishes execution, mark rob entry as done
/////////////////////////////////////////////////////////////

void ROB_mark_ready(ROB *t, Inst_Info inst){
  t->ROB_Entries[inst.dr_tag].ready = true;
}

/////////////////////////////////////////////////////////////
// Find whether the prf (rob entry) is ready
/////////////////////////////////////////////////////////////

bool ROB_check_ready(ROB *t, int tag){
//given an entry determine if it is ready... should we check the ready bit or look at the presence of it's needed operands?
  return t->ROB_Entries[tag].ready;
}


/////////////////////////////////////////////////////////////
// Check if the oldest ROB entry is ready for commit
/////////////////////////////////////////////////////////////

bool ROB_check_head(ROB *t){
  if(t->ROB_Entries[t->head_ptr].valid & t->ROB_Entries[t->head_ptr].ready){
    return true;
  }else{
    return false;
  }
}

/////////////////////////////////////////////////////////////
// For writeback of freshly ready tags, wakeup waiting inst
/////////////////////////////////////////////////////////////

void  ROB_wakeup(ROB *t, int tag){
//i think in this stage we should tell any waiting inst that sources are ready
  //using src1_tag as the index for the rob update the ready(presence) bit of the specific entry
//loop from tag to tail and check each source against tag dest
int i = t->head_ptr;
  if(t->head_ptr == t->tail_ptr){
    i++;
    if(i == MAX_ROB_ENTRIES){
      i=0;
    }
  }
  for(; i!= t->tail_ptr; i++){
    //printf("wakeup loop tail %d idx %d\n",t->tail_ptr, i);
    //printf("src1 %d param %d idx %d",t->ROB_Entries[i].inst.src1_tag, tag, i);
    if(t->ROB_Entries[i].inst.src1_tag == tag)
      t->ROB_Entries[i].inst.src1_ready = true;
    if(t->ROB_Entries[i].inst.src2_tag == tag)
      t->ROB_Entries[i].inst.src2_ready = true;

    //make i circular
    if(i >= MAX_ROB_ENTRIES-1)
      i = -1;

  }
  //no need to check tail index
    if(t->ROB_Entries[t->tail_ptr].inst.src1_tag == tag)
      t->ROB_Entries[t->tail_ptr].inst.src1_ready = true;
    if(t->ROB_Entries[t->tail_ptr].inst.src2_tag == tag)
      t->ROB_Entries[t->tail_ptr].inst.src2_ready = true;


}


/////////////////////////////////////////////////////////////
// Remove oldest entry from ROB (after ROB_check_head)
/////////////////////////////////////////////////////////////

Inst_Info ROB_remove_head(ROB *t){
  Inst_Info inst = t->ROB_Entries[t->head_ptr].inst;
  t->ROB_Entries[t->head_ptr].valid = false;
  t->ROB_Entries[t->head_ptr].ready= false;
  t->ROB_Entries[t->head_ptr].exec = false;
  t->head_ptr++;
  if(t->head_ptr >= MAX_ROB_ENTRIES)//makes the rob circular
    t->head_ptr = 0;
  return inst;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
