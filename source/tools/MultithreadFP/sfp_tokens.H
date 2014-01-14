#ifndef SFP_TASKMGR_H
#define SFP_TASKMGR_H

#include <stack>    // std::stack
#include <vector>   // std::vector
#include <utility>  // std::pair
#include <map>      // std::map
#include "common.H"
#include "sfp_locality_desc.H"
#include "pin.H"

#define DEFAULT_TOKEN 0

typedef unsigned short TToken;

struct TTaskDesc
{
  TTaskDesc(const unsigned int& _taskid) : 
                     taskid(_taskid),
                     parent(0),
                     token(DEFAULT_TOKEN),
                     start_time(0),
                     end_time(0),
                     instrument_enabled(false) {}

  const unsigned int taskid;
  unsigned int parent;
  TToken token;
  TStamp start_time;
  TStamp end_time;
  std::vector<std::pair<TStamp, TStamp> > times;
  TLocalityDesc ldesc;
  bool instrument_enabled;

  inline void enable_instrument() { instrument_enabled = true; }
  inline void disable_instrument() { instrument_enabled = false; }
  inline bool is_instrument_enabled() const { return instrument_enabled; }

  void dump_node(ostream& out) const
  {

    out << "node" << taskid << "[label = \"{{<f0> " << taskid 
        << "|<f1> " << token << "}"
        << "|<f2> " << (start_time )
        << "|<f3> " << (end_time )
        << "}\"];\n";

  }
  
  void dump_edge(ostream& out) const
  {

    out << "\"node" << parent << "\" -> \"node" 
        << taskid << "\";\n";
 
  }
};

class TTokenManager
{

private:

  /**
   * Copy Constructor
   * \param [in] other The object from which to copy construct
   */
  TTokenManager (const TTokenManager& other);

  /**
   * operator=
   * \param [in] other The object from which to copy construct
   */
  void operator= (const TTokenManager& other);

public:
  /**
   * Constructor
   * This is the only means of constructing this object
   */
  TTokenManager()
  {
    /* placeholder for task 0 */
    taskdesc_map[0] = new TTaskDesc(0);

    /* RWMutex initialization */
    PIN_RWMutexInit(&rwlock);
  }

  ~TTokenManager()
  {
    std::map<int, TTaskDesc*>::iterator i;
    for( i = taskdesc_map.begin(); i != taskdesc_map.end(); i++)
    {
      delete i->second;
    }

    /* RWMutex Fini */
    PIN_RWMutexFini(&rwlock);

  }

  inline void ReadLock() { PIN_RWMutexReadLock(&rwlock); }
  inline void WriteLock() { PIN_RWMutexWriteLock(&rwlock); }
  inline void Unlock() { PIN_RWMutexUnlock(&rwlock); }

  inline bool get_token(TTaskDesc* td, const TToken parent)
  {
    for(unsigned int i=0;i<MAX_TOKENS;i++)
    {
      if(task_stack[i].empty())
      {
        current_task[i] = td;
        task_stack[i].push(td);
        td->token = (TToken)i;
        return true;
      }
    }

    current_task[parent] = td;
    task_stack[parent].push(td);
    td->token = parent;
    return false;
  }

  inline bool release_token(TToken token)
  {
    task_stack[token].pop();
    if(task_stack[token].empty())
    {
      current_task[token] = 0;
      return true;
    }
    current_task[token] = task_stack[token].top();
    return false;
  }

  inline TTaskDesc* taskid_to_taskdesc(unsigned int taskid)
  {
    return  taskdesc_map[taskid];
  }

  inline TTaskDesc* token_to_task(TToken token)
  {
    return current_task[token];
  }

  inline TTaskDesc* get_task_descriptor(unsigned int taskid)
  {
    if ( taskdesc_map.find(taskid)!=taskdesc_map.end() ) {
      return taskdesc_map[taskid];
    }

    TTaskDesc* td  = new TTaskDesc(taskid);
    taskdesc_map[taskid] = td;
    return td;
  }

  inline bool is_task_running(unsigned int taskid)
  { 
    if (taskdesc_map.find(taskid) != taskdesc_map.end())
    {
      TTaskDesc* td = taskdesc_map[taskid];
      return td->is_instrument_enabled();
    }
    return false; 
  } 
    
  inline void dump_taskdesc(ostream& out)
  {
    std::map<int, TTaskDesc*>::iterator i;

    out << "digraph g {\n";
    out << "node [shape = record,height=.1];\n";

    for( i = taskdesc_map.begin(); i != taskdesc_map.end(); i++)
    {
      TTaskDesc* td = i->second;
      td->dump_node(out);
    }
    for( i = taskdesc_map.begin(); i != taskdesc_map.end(); i++)
    {
      TTaskDesc* td = i->second;
      td->dump_edge(out);
    }

    out << "}\n";
  }

private:

  TTaskDesc* current_task[MAX_TOKENS];
  std::map<int, TTaskDesc*> taskdesc_map;
  std::stack<TTaskDesc*> task_stack[MAX_TOKENS];
  PIN_RWMUTEX rwlock;

};

#endif
