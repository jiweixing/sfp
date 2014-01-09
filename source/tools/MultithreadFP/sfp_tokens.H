#ifndef SFP_TASKMGR_H
#define SFP_TASKMGR_H

#include <stack>
#include "common.H"
#include "atomic.H"

#define MAX_TOKENS 26
#define DEFAULT_TOKEN 0

typedef unsigned short TToken;

struct TTaskDesc
{
  TTaskDesc(const unsigned int& _taskid) : 
                     taskid(_taskid),
                     parent(0),
                     token(DEFAULT_TOKEN),
                     start_time(0),
                     end_time(0) {}

  const unsigned int taskid;
  unsigned int parent;
  TToken token;
  TStamp start_time;
  TStamp end_time;

  void dump_node()
  {
    std::cout << "node" << taskid << "[label = \"<f0> " 
              << taskid << "|<f1> " << token << "\"];" << std::endl;
  }
  
  void dump_edge()
  {
    std::cout << "\"node" << parent << "\":f0 -> \"node" 
              << taskid << "\":f0;" << std::endl;
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
  TTokenManager() : lock(0)
  {
    taskdesc_map[0] = new TTaskDesc(0);
  }

  inline void Lock() { lock_acquire(&lock); }
  inline void Unlock() { lock_release(&lock); }

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

  inline TToken taskid_to_token(unsigned int taskid)
  {
    return taskdesc_map[taskid]->token;
  }

  inline TTaskDesc* token_to_task(TToken token)
  {
    return current_task[token];
  }

  inline TTaskDesc* get_task_descriptor(unsigned int taskid)
  {
    if ( taskdesc_map.find(taskid) != taskdesc_map.end() ) {
      return taskdesc_map[taskid];
    }

    TTaskDesc* td  = new TTaskDesc(taskid);
    taskdesc_map[taskid] = td;
    return td;
  }

  inline void dump_taskdesc()
  {
    std::map<int, TTaskDesc*>::iterator i;

    std::cout << "digraph g {" << endl;
    std::cout << "node [shape = record,height=.1];" << endl;

    for( i = taskdesc_map.begin(); i != taskdesc_map.end(); i++)
    {
      TTaskDesc* td = i->second;
      td->dump_node();
    }
    for( i = taskdesc_map.begin(); i != taskdesc_map.end(); i++)
    {
      TTaskDesc* td = i->second;
      td->dump_edge();
    }

    std::cout << "}" << std::endl;
  }

private:

  TTaskDesc* current_task[MAX_TOKENS];
  std::map<int, TTaskDesc*> taskdesc_map;
  std::stack<TTaskDesc*> task_stack[MAX_TOKENS];
  sfp_lock_t lock;

};

#endif
