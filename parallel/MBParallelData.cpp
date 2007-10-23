#include "MBParallelData.hpp"
#include "MBParallelComm.hpp"
#include "MBParallelConventions.h"
#include "MBInterface.hpp"

#include <map>

    //! return partition sets; if tag_name is input, gets sets with
    //! that tag name, otherwise uses PARALLEL_PARTITION tag
MBErrorCode MBParallelData::get_partition_sets(MBRange &part_sets,
                                               const char *tag_name) 
{
  MBTag part_tag = 0;
  MBErrorCode result;
  
  if (NULL != tag_name) 
    result = mbImpl->tag_get_handle(tag_name, part_tag);
  else
    result = mbImpl->tag_get_handle(PARALLEL_PARTITION_TAG_NAME, part_tag);
    
  if (MB_SUCCESS != result) return result;
  else if (0 == part_tag) return MB_TAG_NOT_FOUND;
  
  result = mbImpl->get_entities_by_type_and_tag(0, MBENTITYSET, &part_tag, 
                                                NULL, 1, part_sets,
                                                MBInterface::UNION);
  return result;
}
  

    //! get communication interface sets and the processors with which
    //! this processor communicates; sets are sorted by processor
MBErrorCode MBParallelData::get_interface_sets(std::vector<MBEntityHandle> &iface_sets,
                                               std::vector<int> &iface_procs) 
{
#define CONTINUE {result = tmp_result; continue;}
  iface_sets.clear();
  iface_procs.clear();
  
  MBTag proc_tag = 0, procs_tag = 0;
  MBErrorCode result = MB_SUCCESS;
  int my_rank;
  if (parallelComm) my_rank = parallelComm->proc_config().proc_rank();
  else my_rank = mbImpl->proc_rank();

  std::multimap<int, MBEntityHandle> iface_data;

  for (int i = 0; i < 2; i++) {
    MBErrorCode tmp_result;
    
    if (0 == i)
      tmp_result = mbImpl->tag_get_handle(PARALLEL_SHARED_PROC_TAG_NAME, 
                                      proc_tag);
    else
      tmp_result = mbImpl->tag_get_handle(PARALLEL_SHARED_PROCS_TAG_NAME, 
                                      proc_tag);
    if (0 == proc_tag) CONTINUE;

    int tsize;
    tmp_result = mbImpl->tag_get_size(proc_tag, tsize);
    if (0 == tsize || MB_SUCCESS != tmp_result) CONTINUE;
    
    MBRange proc_sets;
    tmp_result = mbImpl->get_entities_by_type_and_tag(0, MBENTITYSET, 
                                                  &proc_tag, NULL, 1,
                                                  proc_sets, MBInterface::UNION);
    if (MB_SUCCESS != tmp_result) CONTINUE;
    
    if (proc_sets.empty()) CONTINUE;
      
    std::vector<int> proc_tags(proc_sets.size()*tsize/sizeof(int));
    tmp_result = mbImpl->tag_get_data(procs_tag, proc_sets, &proc_tags[0]);
    if (MB_SUCCESS != tmp_result) CONTINUE;
    int i;
    MBRange::iterator rit;
    
    for (i = 0, rit = proc_sets.begin(); rit != proc_sets.end(); rit++, i++) {
      for (int j = 0; j < tsize; j++) {
        if (my_rank != proc_tags[2*i+j] && proc_tags[2*i+j] >= 0)
          iface_data.insert(std::pair<int,MBEntityHandle>(proc_tags[2*i+j], *rit));
      }
    }
  }

    // now get the results in sorted order
  std::multimap<int,MBEntityHandle>::iterator mit;
  for (mit = iface_data.begin(); mit != iface_data.end(); mit++)
    iface_procs.push_back((*mit).first),
      iface_sets.push_back((*mit).second);
    
  return result;
}


