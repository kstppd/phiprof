#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <omp.h>
#include <iostream>

#include "timerdata.hpp"
#include "timertree.hpp"
#include "common.hpp"

//initialize profiler, called by first start/initializeTimer. This adds the root timer
TimerTree::TimerTree(){
   std::vector<std::string> group;
   group.push_back("Total");
   //no timer yet
   currentId=-1;
   //mainId will be 0, parent is -1 (does not exist)
   timers.clear();
   timers.push_back(TimerData(NULL, 0, "total", group,""));
   timers[0].start();
}

//initialize a timer, with a particular label belonging to some groups
//returns id of new timer. If timer exists, then that id is returned.
//this function needs to be called by all (active) threads
int TimerTree::initializeTimer(const std::string &label, const std::vector<std::string> &groups, std::string workUnit){
   //check if the global profiler is initialized
   //master + barrier and not single to make sure one at a time is created
   int id;
   
#pragma omp master
   {
      id = getChildId(label); //check if label exists as childtimer
      if(id < 0) {
         //does not exist, let's create it
         id = timers.size(); //id for new timer
         timers.push_back(TimerData(&(timers[currentId]), id, label, groups, workUnit));
      }
   }
#pragma omp barrier   
   return id;

}
   
//start timer, with id
bool TimerTree::start(int id){   
   bool success=true;
   //start timer (currentId = id)
   currentId = timers[id].start();
   return currentId == id;
   
}

//start timer, with label. This function syncronizes OpenMP.
bool TimerTree::start(const std::string &label){
   //If the timer exists, then initializeTimer just returns its id, otherwise it is constructed.
   //Make the timer the current one   
   currentId = initializeTimer(label, std::vector<std::string>(), "");
   return timers[currentId].start();
}



bool TimerTree::stop (const int id)
{
#ifdef DEBUG_PHIPROF_TIMERS         
   if(id != currentId ){
      cerr << "PHIPROF-ERROR: id missmatch in profile::stop Stopping "<< id <<" at level " << timers[currentId].level << endl;
      success=false;
      return success;
   }
#endif            

   currentId = timers[currentId].stop();
   return true;
}

//stop a timer defined by id
bool TimerTree::stop (int id,
                      double workUnits){
   bool success=true;
#ifdef DEBUG_PHIPROF_TIMERS         
   if(id != currentId ){
      cerr << "PHIPROF-ERROR: id missmatch in profile::stop Stopping "<< id <<" at level " << timers[currentId].level << endl;
      success=false;
      return success;
   }
#endif
   currentId = stop(currentId, workUnits);
   return true;
}
//stop a timer defined by id
bool TimerTree::stop (int id,
                      double workUnits,
                      const std::string &workUnitLabel){
   bool success=true;
#ifdef DEBUG_PHIPROF_TIMERS         
   if(id != currentId ){
      cerr << "PHIPROF-ERROR: id missmatch in profile::stop Stopping "<< id <<" at level " << timers[currentId].level << endl;
      success=false;
      return success;
   }
#endif
   currentId = stop(currentId, workUnits, workUnitLabel);
   return true;
}
   

bool TimerTree::stop (const std::string &label)
{
   currentId = timers[currentId].stop();
   return true;
}


//stop with workunits
bool TimerTree::stop (const std::string &label,
                      const double workUnits,
                      const std::string &workUnitLabel){
   currentId = stop(currentId, workUnits, workUnitLabel);
   return true;
   
}


      
//get id number of a timer, return -1 if it does not exist
int TimerTree::getChildId(const std::string &label) const{
   //find child with this id
   for(auto &childId : timers[currentId].getChildIds() ){
      if (timers[childId].getLabel() == label){
         return childId;
      } 
   } 
   //nothing found
   return -1;
}




double TimerTree::getTime(int id) const{
   return timers[id].getAverageTime();
}



double TimerTree::getGroupTime(std::string group, int id) const{
   double groupTime=0.0;
   for(auto &timerGroup : timers[id].getGroups()){
      if(group == timerGroup){
         groupTime = timers[id].getAverageTime();
         return groupTime; // do not collect for children when this is already in group.Avoid double counting
      }
   }
   //recursively collect time data if possibly some children are in
   //group 
   for(auto &childId : timers[id].getChildIds()){
      groupTime += getGroupTime(group, childId);
   }
   return groupTime;
}
         
//Hash value identifying all labels, groups and workunitlabels.
//If any std::strings differ, hash should differ. Computed recursively in the same way as prints
int TimerTree::getHash(int id) const{
   unsigned long hashValue;
   //add hash values from label, workunitlabel and groups. Everything has to match.
   hashValue = timers[id].getHash();
   for(auto &childId : timers[id].getChildIds() ){
      hashValue += timers[childId].getHash();
   }
   
   // MPI_Comm_split needs a non-zero value
   if (hashValue == 0) {
      return 1;
   } else {
      //we return an integer value
      return hashValue%std::numeric_limits<int>::max();
   }
}

//get full hierarchical name for a timer
//can have either the timer-label first (reverse order), or last.
std::string TimerTree::getFullLabel(int id,bool reverse) const{
   //create a label with all hierarchical levels     
   std::vector<std::string> labels;
   while(id>0){
      labels.push_back(timers[id].getLabel());
      id = timers[id].getParentId();
   }
         
   std::string fullLabel;
   if(reverse){
    
      for (auto it=labels.begin() ; it != labels.end(); ++it ){
         fullLabel += *it;
         fullLabel += "\\";   
      }
   }
   else{
      for (auto rit = labels.rbegin() ; rit != labels.rend(); ++rit ){
         fullLabel += "/";
         fullLabel += *rit;
      }
   }
   return fullLabel;
}

//reset timers to zero.
void TimerTree::resetTime(double resetWallTime, int id){
   timers[id].resetTime(resetWallTime);
   for(auto &childId : timers[id].getChildIds() ){
      timers[childId].resetTime(resetWallTime);
   }
}

//remove, e.g., printtime from timings by pushing forward start_time
void TimerTree::shiftActiveStartTime(double shiftTime, int id){
   timers[id].shiftActiveStartTime(shiftTime);
   for(auto &childId : timers[id].getChildIds() ){
      timers[childId].shiftActiveStartTime(shiftTime);
   }
}


      
